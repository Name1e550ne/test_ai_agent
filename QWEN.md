- Пиши осмысленные коментарии к коммитам в стиле conventional commits.
- Никогда не добавляй, не изменяй и не коммить файлы в папке `build/`.
- Папка `build/` является сборочным артефактом.

GIT WORKFLOW RULES:

    Using git add ., git add -A, or git add * is STRICTLY FORBIDDEN.
    Always run git status before every commit and carefully review the output.
    Stage ONLY source code files (e.g., git add src/ include/ CMakeLists.txt).
    If git status shows files from build/, out/, cmake-build-*/, or binary files (.o, .exe, .a, .so), NEVER run git add on them.
    The build/ directory must remain untracked.