@echo off

pushd bin\Debug || exit /b

if exist main.exe (
    main.exe
) else (
    echo main.exe nao encontrado!
)

popd
