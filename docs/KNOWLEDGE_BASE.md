# База знаний проекта

## 1. Описание проекта

**demo_daemon** — это фоновый сервис-демон для Linux Debian, предоставляющий:

- IPC через Unix Domain Socket с JSON-протоколом
- Расширяемую систему команд
- Подсистему фоновых задач
- CLI-клиент для управления
- Интеграцию с systemd

**Цель**: Создать production-quality демон, который служит шаблоном для будущих сервисов.

## 2. Архитектурные решения (ADR)

### ADR-001: Single-threaded epoll event loop для IPC

**Проблема**: Как обрабатывать множественные клиентские соединения?

**Альтернативы**:
1. Thread-per-connection — просто, но плохо масштабируется
2. Thread pool с blocking I/O — сложно, возможны deadlock
3. epoll/IO_uring event loop — эффективно, но сложнее в реализации

**Решение**: Single-threaded epoll event loop + thread pool для фоновых задач.

**Причины**:
- Отсутствие data races на уровне обработки соединений
- Предсказуемое поведение
- Хорошая производительность для умеренного числа клиентов (до 1000)
- Проще отладка и тестирование

### ADR-002: Newline-delimited JSON framing

**Проблема**: Как разделять сообщения в stream socket?

**Альтернативы**:
1. Length-prefixed (4 bytes length + payload)
2. Newline-delimited (JSON per line)
3. EOF-terminated connection per message

**Решение**: Newline-delimited JSON.

**Причины**:
- Простота реализации
- Читаемость при отладке (можно использовать nc, cat)
- Стандартный паттерн (используется во многих проектах)
- Нет проблем с partial reads длины

### ADR-003: nlohmann/json как единственная внешняя зависимость

**Проблема**: Какую JSON библиотеку использовать?

**Альтернативы**:
1. nlohmann/json — header-only, современный API
2. RapidJSON — быстро, но сложный API
3. jsoncpp — устаревший
4. Самописный парсер — рискованно

**Решение**: nlohmann/json через системный пакет.

**Причины**:
- Доступен в Debian как nlohmann-json3-dev
- Header-only опционально
- Современный C++ API
- Хорошая документация

### ADR-004: std::jthread и std::stop_token для управления потоками

**Проблема**: Как безопасно останавливать потоки?

**Альтернативы**:
1. Ручной flag + condition_variable
2. std::thread с ручным join
3. std::jthread с stop_token (C++20)

**Решение**: std::jthread + std::stop_token.

**Причины**:
- RAII для потоков (автоматический join)
- Cooperative cancellation built-in
- Современный C++20 стандарт
- Меньше шансов забыть join

### ADR-005: shared_mutex для CommandRegistry и TaskManager

**Проблема**: Как обеспечить потокобезопасность при частом чтении?

**Альтернативы**:
1. std::mutex — просто, но блокирует всех читателей
2. std::shared_mutex — читатели не блокируют друг друга
3. Lock-free структуры — сложно, избыточно

**Решение**: std::shared_mutex.

**Причины**:
- Чтение происходит гораздо чаще записи
- Читатели не блокируют друг друга
- Доступно в C++17
- Проще чем lock-free

### ADR-006: Foreground режим для systemd

**Проблема**: Как запускать демон — daemonize или foreground?

**Альтернативы**:
1. Классическая daemonization (fork/double-fork)
2. Foreground под управлением systemd
3. Гибридный подход

**Решение**: Foreground режим, systemd управляет процессом.

**Причины**:
- systemd ожидает foreground процессы (Type=simple)
- Проще отладка (видно в terminal)
- Нет сложностей с pidfile, signal forwarding
- Современный best practice для systemd

## 3. IPC Protocol

### Формат сообщений

```
Request:  {"id": <number|string>, "method": "<string>", "params": <object>}
Response: {"id": <same as request>, "result": <any>}
Error:    {"id": <same as request>, "error": {"code": <number>, "message": "<string>"}}
```

Разделитель: `\n` (newline-delimited JSON)

### Коды ошибок протокола

| Code | Название | Описание |
|------|----------|----------|
| -32700 | Parse error | Некорректный JSON (синтаксическая ошибка) |
| -32600 | Invalid Request | Отсутствуют обязательные поля, невалидная структура |
| -32601 | Method not found | Неизвестная команда/метод |
| -32602 | Invalid Params | Неправильные параметры команды |
| -32603 | Internal error | Внутренняя ошибка сервера |
| -32000 | Server busy | Сервер перегружен |
| -32001 | Timeout | Таймаут операции |
| -32002 | NotFound | Ресурс не найден |
| -32003 | Unauthorized | Требуется авторизация |
| -32004 | Forbidden | Доступ запрещен |

### Максимальный размер сообщения

По умолчанию: **1 MiB** (1048576 байт).

При превышении лимита сообщение отбрасывается, возвращается ошибка ParseError.

### Поведение при ошибках

| Ситуация | Реакция |
|----------|---------|
| Некорректный JSON | Вернуть `{"id": null, "error": {"code": -32700, ...}}`, соединение НЕ закрывать |
| Unknown method | Вернуть ошибку MethodNotFound с id из запроса |
| Missing id в request | Обработать как notification (без ответа) или вернуть InvalidRequest |
| Разрыв соединения клиентом | Корректно закрыть fd, удалить сессию |
| Partial read | Накопить данные в буфере, ждать продолжения |
| Partial write | Продолжить запись, использовать non-blocking I/O |

### Таймауты

- Чтение: настраиваемый таймаут через SO_RCVTIMEO
- Запись: настраиваемый таймаут через SO_SNDTIMEO
- Рекомендуется: 30 секунд по умолчанию

### Примеры сообщений

```json
// Ping запрос
{"id":1,"method":"ping","params":{}}

// Ping ответ
{"id":1,"result":{"status":"ok","timestamp":"2025-01-15T10:30:00Z"}}

// Ошибка метода
{"id":2,"error":{"code":-32601,"message":"Method not found: unknown_method"}}

// Notification (без ответа)
{"method":"tasks.add","params":{"type":"heartbeat","interval":5}}

// Ошибка парсинга
{"id":null,"error":{"code":-32700,"message":"Invalid JSON payload"}}
```

### Реализация в коде

- `include/demo_daemon/ipc/json_protocol.hpp` — интерфейс и типы
- `src/ipc/json_protocol.cpp` — реализация парсера/сериализатора
- `JsonProtocolParser::try_parse()` — инкрементальный парсинг с framing
- `JsonProtocolParser::serialize()` — сериализация в строку с '\n'

## 4. Команды

### Built-in команды

| Команда | Параметры | Описание |
|---------|-----------|----------|
| `ping` | {} | Проверка связи |
| `status` | {} | Статус демона (uptime, задачи, клиенты) |
| `shutdown` | {delay_seconds: int} | Остановка демона |
| `commands.list` | {} | Список доступных команд |
| `tasks.list` | {} | Список фоновых задач |
| `tasks.add` | {type: string, ...} | Добавить задачу |
| `tasks.start` | {id: string} | Запустить задачу |
| `tasks.stop` | {id: string} | Остановить задачу |
| `tasks.remove` | {id: string} | Удалить задачу |

### Добавление новой команды

1. Создать класс наследующий `ICommand`:
```cpp
class MyCommand : public ICommand {
public:
    std::string name() const override { return "my.command"; }
    std::string description() const override { return "Description"; }
    nlohmann::json execute(const ServiceContext& ctx, const nlohmann::json& params) override {
        return {{"result", "success"}};
    }
};
```

2. Зарегистрировать в main.cpp:
```cpp
registry.registerCommand(std::make_unique<MyCommand>());
```

## 5. Фоновые задачи

### Состояния задачи

```
Pending → Running → Stopping → Stopped
              ↓
            Failed
```

### Интерфейс ITask

```cpp
class ITask {
public:
    virtual ~ITask() = default;
    [[nodiscard]] virtual std::string id() const = 0;
    [[nodiscard]] virtual std::string type() const = 0;
    virtual void start(std::stop_token stop_token) = 0;
    virtual void request_stop() = 0;
    [[nodiscard]] virtual TaskInfo info() const = 0;
};
```

### Добавление новой задачи

1. Создать класс наследующий `ITask`:
```cpp
class MyTask : public ITask {
public:
    void start(std::stop_token stop_token) override {
        while (!stop_token.stop_requested()) {
            // работа
        }
    }
    // ... остальные методы
};
```

2. Использовать через CLI:
```bash
demo_daemon_cli tasks add --type my_task --param value
```

## 6. Сборка

### Зависимости

```bash
sudo apt install build-essential cmake libnlohmann-json3-dev
```

### Команды сборки

```bash
# Debug сборка
cmake -B build -S . -DCMAKE_BUILD_TYPE=Debug
cmake --build build

# Release сборка
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build

# Со sanitizers
cmake -B build -S . -DENABLE_SANITIZERS=ON
cmake --build build

# Со tests
cmake -B build -S . -DBUILD_TESTS=ON
cmake --build build
ctest --test-dir build
```

### Опции CMake

| Опция | По умолчанию | Описание |
|-------|--------------|----------|
| CMAKE_BUILD_TYPE | Debug | Тип сборки |
| ENABLE_SANITIZERS | OFF | Включить ASan/UBSan/TSan |
| BUILD_TESTS | OFF | Включить тесты |

## 7. Запуск

### Демон

```bash
# Foreground режим (для systemd)
./build/src/daemon/demo_daemon --foreground

# Свой путь к socket
./build/src/daemon/demo_daemon --socket-path /tmp/my.sock

# Логирование
./build/src/daemon/demo_daemon --log-level debug
```

### CLI

```bash
# Ping
demo_daemon_cli ping

# Status
demo_daemon_cli status

# Shutdown
demo_daemon_cli shutdown

# List commands
demo_daemon_cli commands list

# Tasks
demo_daemon_cli tasks list
demo_daemon_cli tasks add --type heartbeat --interval 5
demo_daemon_cli tasks stop --id <task_id>
```

### Переменные окружения

| Переменная | Описание |
|------------|----------|
| DEMO_DAEMON_SOCKET | Путь к socket (переопределяет дефолт) |
| DEMO_DAEMON_LOG_LEVEL | Уровень логирования |

## 8. Тестирование

### Smoke test

```bash
# Запустить демон в background
./build/src/daemon/demo_daemon --foreground &
PID=$!

# Подождать старта
sleep 1

# Проверить ping
./build/src/cli/demo_daemon_cli ping

# Остановить
kill $PID
wait $PID
```

### Sanitizers

```bash
# ASan/LSan
cmake -B build -S . -DENABLE_SANITIZERS=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build
./build/src/daemon/demo_daemon --foreground

# TSan (отдельная сборка)
cmake -B build-tsan -S . -DENABLE_SANITIZERS=ON -DUSE_TSAN=ON
cmake --build build-tsan
```

### Integration tests

```bash
cd tests
./run_integration_tests.sh
```

## 9. Known issues

### Текущие ограничения

1. **std::format**: Может быть недоступен в GCC 12, используется fallback на stringstream
2. **TSan + ASan**: Несовместимы, требуют раздельных сборок
3. **Abstract namespace sockets**: Не поддерживаются, только filesystem paths
4. **SSL/TLS**: Не реализовано, IPC локальный без шифрования

### Планы на будущее

- [ ] Конфигурационный файл (YAML/JSON)
- [ ] Prometheus metrics endpoint
- [ ] Structured logging (JSON format)
- [ ] Hot reload конфигурации по SIGHUP

## 10. Инструкции для агентов

### Что нельзя ломать

1. **Модульность**: Ядро (`demo_daemon_core`) должно оставаться независимым
2. **RAII**: Никаких raw new/delete для владения
3. **Потокобезопасность**: Все shared mutable state должен быть защищен
4. **IPC контракт**: Формат сообщений не менять без веской причины

### Какие паттерны использовать

1. **Dependency Injection**: Через конструкторы
2. **RAII**: Для всех ресурсов (fd, mutex, threads)
3. **Factory**: Для создания команд и задач
4. **Registry**: Для хранения команд
5. **Observer**: Для событий (если понадобится)

### Куда смотреть первым делом

1. `docs/PLAN.md` — общий план и архитектура
2. `docs/STATUS.md` — текущее состояние
3. `include/demo_daemon/` — публичные интерфейсы
4. `src/core/` — базовые компоненты

### При добавлении функциональности

1. Обновить интерфейс (если нужно)
2. Реализовать в отдельном файле
3. Добавить тесты
4. Обновить документацию
5. Обновить KNOWLEDGE_BASE.md

### При изменении архитектуры

1. Записать ADR в этот файл
2. Обновить PLAN.md
3. Получить подтверждение пользователя
