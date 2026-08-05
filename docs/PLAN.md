# План реализации

## 1. Цель проекта

Создать production-quality фоновый сервис-демон для Linux Debian, который:

- Работает как долгоживущий системный сервис с поддержкой graceful shutdown
- Поддерживает IPC через Unix Domain Socket с JSON-протоколом
- Предоставляет расширяемую систему команд и фоновых задач
- Имеет CLI-клиент для взаимодействия
- Написан на C++20 в объектно-ориентированном стиле
- Является модульным, потокобезопасным и устойчивым к утечкам памяти

Итоговый результат: демон `demo_daemon`, CLI `demo_daemon_cli`, systemd unit файл, полная документация.

## 2. Допущения и ограничения

### Системные требования
- Debian 12 (Bookworm) или новее
- g++ 12+ с полной поддержкой C++20
- CMake 3.22+
- systemd для управления сервисом

### Технические допущения
- Используем стандартную библиотеку C++20 без экспериментальных расширений
- `std::format` доступен в GCC 13+, для GCC 12 используем fallback на stringstream
- nlohmann/json подключаем через системный пакет `nlohmann-json3-dev`
- Минимум внешних зависимостей — только nlohmann/json
- Демон работает в foreground режиме под управлением systemd
- Unix Domain Socket создается в `/run/demo_daemon/demo_daemon.sock` для root или `$XDG_RUNTIME_DIR/demo_daemon.sock` для пользователя
- Максимальный размер JSON сообщения: 1 MiB
- Таймаут чтения/записи: 30 секунд по умолчанию
- Максимум одновременных клиентских соединений: 100 (настраиваемо)

### Ограничения
- Не поддерживаем Windows или другие ОС
- Не поддерживаем abstract namespace Unix sockets
- Не реализуем полную daemonization (fork/double-fork) — полагаемся на systemd
- SSL/TLS шифрование не требуется для локального IPC

## 3. Архитектура

### Компоненты системы

```
┌─────────────────────────────────────────────────────────────┐
│                      demo_daemon                            │
├─────────────────────────────────────────────────────────────┤
│  ┌─────────────┐  ┌──────────────┐  ┌──────────────────┐   │
│  │ Signal      │  │ Logger       │  │ Config           │   │
│  │ Handler     │  │ (thread-safe)│  │ (optional)       │   │
│  └─────────────┘  └──────────────┘  └──────────────────┘   │
├─────────────────────────────────────────────────────────────┤
│  ┌──────────────────────────────────────────────────────┐   │
│  │              UnixSocketServer                        │   │
│  │  ┌─────────────┐  ┌─────────────┐  ┌──────────────┐ │   │
│  │  │ AcceptLoop  │  │ Connection  │  │ JsonProtocol │ │   │
│  │  │ (epoll)     │  │ Pool        │  │ Parser       │ │   │
│  │  └─────────────┘  └─────────────┘  └──────────────┘ │   │
│  └──────────────────────────────────────────────────────┘   │
├─────────────────────────────────────────────────────────────┤
│  ┌─────────────────┐         ┌──────────────────────────┐   │
│  │ CommandRegistry │◄────────│ Built-in Commands        │   │
│  │                 │         │ - ping                   │   │
│  │                 │         │ - status                 │   │
│  │                 │         │ - shutdown               │   │
│  │                 │         │ - commands.list          │   │
│  │                 │         │ - tasks.*                │   │
│  └─────────────────┘         └──────────────────────────┘   │
├─────────────────────────────────────────────────────────────┤
│  ┌─────────────────┐         ┌──────────────────────────┐   │
│  │ TaskManager     │◄────────│ Background Tasks         │   │
│  │                 │         │ - HeartbeatTask          │   │
│  │                 │         │ - User-defined tasks     │   │
│  └─────────────────┘         └──────────────────────────┘   │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
                    ┌─────────────────┐
                    │ demo_daemon_cli │
                    │ (CLI client)    │
                    └─────────────────┘
```

### Слои архитектуры

1. **Core Layer** (`demo_daemon_core` library)
   - Базовые утилиты, типы, концепты
   - Logger (потокобезопасный)
   - Result/Error types
   - JSON protocol abstraction

2. **IPC Layer**
   - UnixSocketServer (epoll-based event loop)
   - Connection manager
   - JSON framing (newline-delimited)
   - Request/Response handling

3. **Command Layer**
   - ICommand interface
   - CommandRegistry
   - Built-in commands
   - Command context и execution

4. **Task Layer**
   - ITask interface
   - TaskManager (thread pool)
   - Task lifecycle management
   - Cooperative cancellation

5. **Application Layer** (`demo_daemon` executable)
   - Main entry point
   - Signal handling
   - Composition root
   - Graceful shutdown orchestration

6. **CLI Layer** (`demo_daemon_cli` executable)
   - Socket client
   - Command-line parsing
   - Human-readable output formatting

### Потоки

```
Main Thread
├── Signal handling (atomic flags)
├── Event loop (epoll_wait)
└── Shutdown coordination

Worker Thread Pool (для задач)
├── Task execution threads
├── Cooperative cancellation via stop_token
└── Graceful termination

Per-connection threads (опционально, если не используем epoll)
└── Или обработка в event loop без дополнительных потоков
```

**Решение**: Используем single-threaded epoll event loop для IPC + thread pool для фоновых задач. Это обеспечивает:
- Отсутствие data races на уровне обработки соединений
- Масштабируемость для долгих операций через task pool
- Простоту отладки и понимания

### Реестр команд

```cpp
class ICommand {
public:
    virtual ~ICommand() = default;
    [[nodiscard]] virtual std::string name() const = 0;
    [[nodiscard]] virtual std::string description() const = 0;
    [[nodiscard]] virtual nlohmann::json execute(
        const ServiceContext& ctx,
        const nlohmann::json& params
    ) = 0;
};

class CommandRegistry {
public:
    void registerCommand(std::unique_ptr<ICommand> cmd);
    [[nodiscard]] std::optional<std::reference_wrapper<ICommand>> get(const std::string& name);
    [[nodiscard]] nlohmann::json listCommands() const;
private:
    std::unordered_map<std::string, std::unique_ptr<ICommand>> commands_;
    mutable std::shared_mutex mutex_;
};
```

### Менеджер фоновых задач

```cpp
enum class TaskState { Pending, Running, Stopping, Stopped, Failed };

struct TaskInfo {
    std::string id;
    std::string type;
    TaskState state;
    std::chrono::steady_clock::time_point created_at;
    std::optional<std::chrono::steady_clock::time_point> started_at;
    std::optional<std::chrono::steady_clock::time_point> stopped_at;
    std::optional<std::string> error;
};

class ITask {
public:
    virtual ~ITask() = default;
    [[nodiscard]] virtual std::string id() const = 0;
    [[nodiscard]] virtual std::string type() const = 0;
    virtual void start(std::stop_token stop_token) = 0;
    virtual void request_stop() = 0;
    [[nodiscard]] virtual TaskInfo info() const = 0;
};

class TaskManager {
public:
    std::string addTask(std::unique_ptr<ITask> task);
    void removeTask(const std::string& id);
    void startTask(const std::string& id);
    void stopTask(const std::string& id);
    [[nodiscard]] std::vector<TaskInfo> listTasks() const;
    void shutdown(); // остановить все задачи gracefully
private:
    std::unordered_map<std::string, std::shared_ptr<ITask>> tasks_;
    std::jthread worker_thread_;
    mutable std::shared_mutex mutex_;
};
```

### Логирование

Потокобезопасный логгер с уровнями:

```cpp
enum class LogLevel { Trace, Debug, Info, Warn, Error };

class Logger {
public:
    static Logger& instance();
    void setLevel(LogLevel level);
    void log(LogLevel level, std::string_view msg);
    // Macro-friendly interface
private:
    std::ostream& stream_;
    std::mutex mutex_;
    LogLevel min_level_;
};

#define LOG_TRACE(msg) Logger::instance().log(LogLevel::Trace, msg)
#define LOG_DEBUG(msg) Logger::instance().log(LogLevel::Debug, msg)
#define LOG_INFO(msg)  Logger::instance().log(LogLevel::Info, msg)
#define LOG_WARN(msg)  Logger::instance().log(LogLevel::Warn, msg)
#define LOG_ERROR(msg) Logger::instance().log(LogLevel::Error, msg)
```

### Обработка сигналов

```cpp
class SignalHandler {
public:
    using HandlerFunc = void(*)(int);
    
    static SignalHandler& instance();
    void setup();
    bool should_stop() const;
    void request_stop();
    
private:
    static void handle_signal(int signum);
    static int signal_pipe_[2];
    std::atomic<bool> should_stop_{false};
};
```

- Используем `signalfd` или async-signal-safe pipe для уведомления main loop
- В handler только пишем байт в pipe, установка atomic flag
- Main loop читает из pipe через epoll и выполняет graceful shutdown

### Управление памятью

- Все владеющие указатели: `std::unique_ptr` или `std::shared_ptr`
- Наблюдающие ссылки: `std::weak_ptr` или сырые указатели (без владения)
- Запрет raw new/delete для владения
- RAII для всех ресурсов (fd, mutex guards, threads)
- Move semantics для передачи владения
- Избегание циклических ссылок через weak_ptr

### Error Handling

```cpp
template<typename T>
class Result {
public:
    static Result ok(T value);
    static Result error(std::string message, int code = -1);
    
    [[nodiscard]] bool is_ok() const;
    [[nodiscard]] bool is_error() const;
    [[nodiscard]] T& value();
    [[nodiscard]] const std::string& error_message() const;
    [[nodiscard]] int error_code() const;
    
private:
    std::variant<T, ErrorData> data_;
};

// Для void результатов
using Status = Result<void>;
```

## 4. Контракт IPC

### Формат сообщений

Newline-delimited JSON over Unix Domain Socket (SOCK_STREAM).

Одно сообщение = одна строка JSON, завершенная `\n`.

**Запрос (Request):**
```json
{"id": <number|string>, "method": "<string>", "params": <object>}
```

**Успешный ответ (Success Response):**
```json
{"id": <number|string>, "result": <any>}
```

**Ошибка (Error Response):**
```json
{"id": <number|string>, "error": {"code": <number>, "message": "<string>"}}
```

### Примеры

**Ping:**
```
Client: {"id":1,"method":"ping","params":{}}
Server: {"id":1,"result":{"status":"ok","timestamp":"2025-01-15T10:30:00Z"}}
```

**Status:**
```
Client: {"id":2,"method":"status","params":{}}
Server: {"id":2,"result":{"version":"0.1.0","uptime_seconds":3600,"active_clients":2,"tasks":[{"id":"t1","type":"heartbeat","state":"Running"}]}}
```

**Shutdown:**
```
Client: {"id":3,"method":"shutdown","params":{"delay_seconds":0}}
Server: {"id":3,"result":{"status":"shutting_down"}}
```

**Unknown Method:**
```
Client: {"id":4,"method":"unknown","params":{}}
Server: {"id":4,"error":{"code":-32601,"message":"Method not found"}}
```

**Invalid JSON:**
```
Client: {invalid json}
Server: {"id":null,"error":{"code":-32700,"message":"Parse error: invalid JSON"}}
```

### Коды ошибок (JSON-RPC style)

| Code | Message | Описание |
|------|---------|----------|
| -32700 | Parse error | Некорректный JSON |
| -32600 | Invalid Request | Missing required fields |
| -32601 | Method not found | Неизвестная команда |
| -32602 | Invalid Params | Неправильные параметры |
| -32603 | Internal error | Внутренняя ошибка сервера |
| -32000 | Server busy | Сервер перегружен |
| -32001 | Task not found | Задача не найдена |
| -32002 | Task already exists | Задача уже существует |

### Поведение при ошибках

| Ситуация | Поведение |
|----------|-----------|
| Некорректный JSON | Вернуть error response, закрыть соединение если повторяется |
| Unknown method | Вернуть error response с code -32601 |
| Invalid params | Вернуть error response с code -32602 |
| Internal error | Вернуть error response с code -32603, залогировать ошибку |
| Превышен размер сообщения | Закрыть соединение, залогировать warning |
| Разрыв соединения клиентом | Очистить сессию, продолжить работу |
| Таймаут чтения/записи | Закрыть соединение, залогировать debug message |

### Технические ограничения

- Максимальный размер сообщения: 1 MiB (настраиваемо)
- Таймаут чтения: 30 секунд
- Таймаут записи: 30 секунд
- Partial read/write: обрабатывать корректно через буферизацию
- Framing: newline (`\n`) как разделитель сообщений

## 5. Модульность и расширяемость

### Добавление новой команды

1. Создать класс, наследующий `ICommand`:
```cpp
class MyCustomCommand : public ICommand {
public:
    std::string name() const override { return "my.command"; }
    std::string description() const override { return "Does something useful"; }
    nlohmann::json execute(const ServiceContext& ctx, const nlohmann::json& params) override {
        // реализация
        return {{"result", "done"}};
    }
};
```

2. Зарегистрировать команду в `main.cpp`:
```cpp
registry.registerCommand(std::make_unique<MyCustomCommand>());
```

### Добавление фоновой задачи

1. Создать класс, наследующий `ITask`:
```cpp
class MyPeriodicTask : public ITask {
public:
    std::string id() const override { return id_; }
    std::string type() const override { return "my_periodic"; }
    
    void start(std::stop_token stop_token) override {
        while (!stop_token.stop_requested()) {
            // полезная работа
            std::this_thread::sleep_for(std::chrono::seconds(interval_));
        }
    }
    
    void request_stop() override { /* cleanup */ }
    TaskInfo info() const override { /* вернуть информацию */ }
    
private:
    std::string id_;
    std::chrono::seconds interval_{5};
};
```

2. Добавить factory в TaskManager или зарегистрировать тип.

### Добавление нового модуля

1. Создать отдельный header/source файлы
2. Использовать dependency injection через конструкторы
3. Интегрировать в composition root в `main.cpp`

### Расширение обработчиков событий

- Использовать observer pattern через callbacks
- Registrar событий в ядре
- Модули подписываются на интересующие события

## 6. Потокобезопасность

### Защищенные данные

| Данные | Защита | Обоснование |
|--------|--------|-------------|
| CommandRegistry::commands_ | std::shared_mutex | Частое чтение, редкая запись |
| TaskManager::tasks_ | std::shared_mutex | Частое чтение из IPC, запись при добавлении/удалении |
| Logger::buffer_ | std::mutex | Критическая секция минимальна |
| Connection state | Локально в event loop | Single-threaded обработка |

### Atomic переменные

| Переменная | Тип | Назначение |
|------------|-----|------------|
| SignalHandler::should_stop_ | std::atomic<bool> | Флаг остановки от сигнала |
| Daemon::running_ | std::atomic<bool> | Флаг работы демона |
| Task::state_ | std::atomic<TaskState> | Состояние задачи |

### Остановка потоков

- Используем `std::jthread` с `std::stop_token`
- Cooperative cancellation:定期检查 `stop_token.stop_requested()`
- Graceful shutdown: сначала остановить прием новых задач, затем дождаться завершения текущих

### Избегание deadlock

1. Порядок блокировок: всегда захватываем mutex в одинаковом порядке
2. Не держать mutex во время blocking I/O
3. Не вызывать user callbacks под mutex
4. Использовать `std::lock_guard` / `std::scoped_lock` вместо ручного lock/unlock
5. Избегать вложенных блокировок там, где возможно

### Использование std::jthread

```cpp
std::jthread worker([](std::stop_token st) {
    while (!st.stop_requested()) {
        // работа
        if (condition.wait_for(lock, std::chrono::milliseconds(100), [&]{ return st.stop_requested() || work_available; })) {
            break;
        }
    }
});
// Автоматический join и запрос остановки при выходе из scope
```

## 7. Управление памятью

### Принципы

1. **RAII для всего**: ресурсы освобождаются в деструкторах
2. **Smart pointers**:
   - `std::unique_ptr` — единоличное владение (по умолчанию)
   - `std::shared_ptr` — общее владение (только когда необходимо)
   - `std::weak_ptr` — наблюдение без владения
3. **Запрет raw new/delete**: использовать `std::make_unique` / `std::make_shared`
4. **Move semantics**: передавать владение через move, не копировать без нужды
5. **Избегание циклов**: shared_ptr → weak_ptr для обратных ссылок

### Паттерны владения

```cpp
// Factory создает и возвращает unique_ptr
std::unique_ptr<ICommand> createCommand();

// Registry забирает владение
void registerCommand(std::unique_ptr<ICommand> cmd);

// TaskManager хранит shared_ptr для задач, которые могут жить в отдельных потоках
std::unordered_map<std::string, std::shared_ptr<ITask>> tasks_;

// Weak reference для callbacks
std::weak_ptr<ITask> task_weak = task_shared;
if (auto task = task_weak.lock()) {
    task->doSomething();
}
```

### Предотвращение проблем

| Проблема | Решение |
|----------|---------|
| Циклические ссылки | weak_ptr для обратных ссылок |
| Dangling references | Не хранить ссылки на временные объекты |
| Double free | unique_ptr исключает возможность |
| Memory leak | RAII, smart pointers, sanitizer проверка |
| Use after free | Sanitizers, careful lifetime management |

## 8. Пошаговый план

### Шаг 1: Каркас проекта и CMake

**Цель**: Создать структуру проекта и базовую систему сборки.

**Файлы**:
- `CMakeLists.txt` (root)
- `src/CMakeLists.txt`
- `src/core/CMakeLists.txt`
- `src/daemon/CMakeLists.txt`
- `src/cli/CMakeLists.txt`
- `include/demo_daemon/version.hpp`
- `.gitignore`

**Ожидаемый результат**: Пустой проект собирается через CMake, создаются targets `demo_daemon_core`, `demo_daemon`, `demo_daemon_cli`.

**Definition of Done**:
- `cmake -B build -S .` выполняется без ошибок
- `cmake --build build` компилирует пустые targets
- `ENABLE_SANITIZERS` опция работает

**Проверка**: Запустить cmake и make, проверить создание binaries.

**Риски**: Версии CMake/g++ могут отличаться на разных системах.

---

### Шаг 2: Базовые утилиты, ошибки, логирование

**Цель**: Реализовать фундаментальные компоненты.

**Файлы**:
- `include/demo_daemon/core/result.hpp`
- `include/demo_daemon/core/logger.hpp`
- `src/core/logger.cpp`
- `include/demo_daemon/core/concepts.hpp`
- Тесты для logger и result

**Ожидаемый результат**: Working logger с уровнями, Result/Either тип для error handling.

**Definition of Done**:
- Logger пишет в stderr с timestamp и уровнем
- Result<T> поддерживает ok/error состояния
- Unit тесты проходят

**Проверка**: Запустить тесты, проверить вывод логов.

**Риски**: std::format может быть недоступен в GCC 12.

---

### Шаг 3: JSON protocol abstraction

**Цель**: Реализовать парсинг и сериализацию JSON сообщений.

**Файлы**:
- `include/demo_daemon/protocol/json_protocol.hpp`
- `src/core/json_protocol.cpp`
- `include/demo_daemon/protocol/message.hpp`

**Ожидаемый результат**: Parser запросов/ответов, validation, error codes.

**Definition of Done**:
- Парсинг newline-delimited JSON
- Валидация полей id, method, params
- Генерация error responses

**Проверка**: Unit тесты с различными JSON payload.

**Риски**: nlohmann/json может отсутствовать в системе.

---

### Шаг 4: Unix Domain Socket server

**Цель**: Реализовать IPC сервер на базе epoll.

**Файлы**:
- `include/demo_daemon/ipc/unix_socket_server.hpp`
- `src/ipc/unix_socket_server.cpp`
- `include/demo_daemon/ipc/connection.hpp`
- `include/demo_daemon/ipc/fd_guard.hpp` (RAII для fd)

**Ожидаемый результат**: Сервер слушает socket, принимает соединения, читает/пишет данные.

**Definition of Done**:
- Создание socket с правильными правами
- epoll-based event loop
- Обработка множественных клиентов
- Graceful закрытие соединений

**Проверка**: Ручной тест через nc или простой клиент.

**Риски**: epoll сложен в отладке, edge cases с закрытием fd.

---

### Шаг 5: Command registry и built-in команды

**Цель**: Реализовать систему команд.

**Файлы**:
- `include/demo_daemon/command/icommand.hpp`
- `include/demo_daemon/command/command_registry.hpp`
- `src/command/command_registry.cpp`
- `src/command/builtin_commands.cpp` (ping, status, shutdown, commands.list)

**Ожидаемый результат**: Работающий registry с 4 built-in командами.

**Definition of Done**:
- Регистрация команд по имени
- Выполнение команд через IPC
- Возврат JSON результата

**Проверка**: CLI вызывает ping, status, получает ответы.

**Риски**: Потокобезопасность registry при concurrent запросах.

---

### Шаг 6: Task manager и интерфейс фоновых задач

**Цель**: Реализовать подсистему фоновых задач.

**Файлы**:
- `include/demo_daemon/task/itask.hpp`
- `include/demo_daemon/task/task_manager.hpp`
- `src/task/task_manager.cpp`
- `include/demo_daemon/task/task_info.hpp`

**Ожидаемый результат**: TaskManager управляет lifecycle задач.

**Definition of Done**:
- Добавление/удаление задач
- Старт/стоп задач
- Отчет о состоянии задач
- Thread pool для выполнения

**Проверка**: Программное создание и остановка задач.

**Риски**: Deadlocks при остановке задач, утечки потоков.

---

### Шаг 7: Пример фоновой задачи

**Цель**: Показать расширяемость системы задач.

**Файлы**:
- `src/task/heartbeat_task.cpp`
- `include/demo_daemon/task/heartbeat_task.hpp`
- Команда tasks.add, tasks.start, tasks.stop, tasks.list

**Ожидаемый результат**: HeartbeatTask периодически логирует статус.

**Definition of Done**:
- HeartbeatTask реализует ITask
- Запускается через CLI
- Корректно останавливается

**Проверка**: CLI добавляет задачу, наблюдает в status.

**Риски**: Задача может заблокироваться, нужен timeout.

---

### Шаг 8: CLI-клиент

**Цель**: Реализовать клиентскую утилиту.

**Файлы**:
- `src/cli/main.cpp`
- `src/cli/cli_client.hpp`
- `src/cli/output_formatter.hpp`

**Ожидаемый результат**: CLI подключается, отправляет команды, показывает результат.

**Definition of Done**:
- Подключение к socket
- Отправка JSON запросов
- Парсинг ответов
- Human-readable вывод
- Правильные exit codes

**Проверка**: `demo_daemon_cli ping` возвращает ok.

**Риски**: Таймауты, обработка ошибок подключения.

---

### Шаг 9: Обработка сигналов и graceful shutdown

**Цель**: Корректная остановка демона.

**Файлы**:
- `include/demo_daemon/core/signal_handler.hpp`
- `src/core/signal_handler.cpp`
- Интеграция в main loop

**Ожидаемый результат**: Демон останавливается по SIGTERM/SIGINT.

**Definition of Done**:
- signalfd или pipe для signal notification
- Остановка event loop
- Остановка всех задач
- Закрытие socket
- Exit code 0

**Проверка**: `kill -TERM <pid>` корректно останавливает.

**Риски**: Signal safety, race conditions.

---

### Шаг 10: Thread safety hardening

**Цель**: Убедиться в отсутствии data races.

**Файлы**: Изменения в существующих файлах по мере выявления проблем.

**Ожидаемый результат**: TSan clean build.

**Definition of Done**:
- Сборка с -fsanitize=thread
- Запуск тестов без warnings
- Документирование всех protected sections

**Проверка**: TSan build + integration tests.

**Риски**: TSan может давать false positives.

---

### Шаг 11: Sanitizers, warnings, тесты

**Цель**: Полная проверка качества кода.

**Файлы**:
- CMake options для sanitizers
- Integration test script
- Дополнительные unit тесты

**Ожидаемый результат**: ASan/LSan/UBSan clean, warnings-free compilation.

**Definition of Done**:
- ENABLE_SANITIZERS=ON работает
- Все тесты проходят
- -Wall -Wextra -Wpedantic без warnings

**Проверка**: `cmake -DENABLE_SANITIZERS=ON && ctest`

**Риски**: Разные sanitizers могут конфликтовать.

---

### Шаг 12: systemd unit и документация

**Цель**: Production deployment ready.

**Файлы**:
- `deploy/systemd/demo_daemon.service`
- `README.md`
- `docs/PROTOCOL.md`
- `docs/EXTENDING.md`
- `docs/DEPLOY.md`

**Ожидаемый результат**: Готовый к установке пакет с документацией.

**Definition of Done**:
- systemd unit файл работает
- README полный и актуальный
- Инструкции по deploy проверены

**Проверка**: Установка и запуск через systemctl.

**Риски**: Различия в systemd версиях.

---

### Шаг 13: Финальная проверка и README

**Цель**: Завершение проекта.

**Файлы**: Обновление всей документации, финальный review.

**Ожидаемый результат**: Production-ready проект.

**Definition of Done**:
- Все критерии готовности выполнены
- Документация актуальна
- Smoke test проходит

**Проверка**: Полная сборка и тест с нуля.

**Риски**: Недочеты в документации.

## 9. Критерии готовности

Проект считается готовым, когда:

### Сборка
- [ ] Собирается через CMake без ошибок на Debian 12
- [ ] Опции ENABLE_SANITIZERS и BUILD_TESTS работают
- [ ] Нет compiler warnings с -Wall -Wextra -Wpedantic

### Тесты
- [ ] Smoke test: демон запускается, CLI делает ping
- [ ] Демон останавливается по SIGTERM
- [ ] Unknown command возвращает JSON error
- [ ] Invalid JSON возвращает JSON error
- [ ] ASan/LSan не обнаруживают утечек
- [ ] TSan не обнаруживает data races (если доступно)

### Функциональность
- [ ] Сервис запускается в foreground
- [ ] CLI подключается через Unix socket
- [ ] Все built-in команды работают
- [ ] Фоновые задачи добавляются и управляются
- [ ] Graceful shutdown отрабатывает

### Документация
- [ ] README.md полный и актуальный
- [ ] docs/PROTOCOL.md описывает IPC
- [ ] docs/EXTENDING.md показывает примеры расширения
- [ ] systemd unit файл протестирован

### Код
- [ ] Нет raw new/delete для владения
- [ ] Все ресурсы через RAII
- [ ] Потоки безопасно останавливаются
- [ ] Нет глобального mutable state
- [ ] Follows SOLID principles
