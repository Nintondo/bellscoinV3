#!/bin/bash

# Запускаем python3 auxpow_mining.py
OUTPUT=$(python3 auxpow_mining.py 2>&1)

# Извлекаем директорию с тестом из вывода
TEST_DIR=$(echo "$OUTPUT" | grep -oP 'TestFramework $$\$\$ERROR$$\: Test failed. Test logging available at \/\K[^\s]+')

# Получаем номер запуска скрипта
N=$(ls -d logs* | wc -l)

# Вызываем скрипт combine_logs.py и сохраняем вывод в файл logsN.txt
/home/dmatsiukhov/git_repos/bellscoin/test/functional/combine_logs.py "$TEST_DIR" > "logs$N.txt"