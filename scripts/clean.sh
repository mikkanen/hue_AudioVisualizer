#!/bin/bash

# Tämä rivi seuraa symbolista linkkiä alkuperäiseen tiedostoon
SOURCE="${BASH_SOURCE[0]}"
while [ -h "$SOURCE" ]; do
  DIR="$( cd -P "$( dirname "$SOURCE" )" >/dev/null 2>&1 && pwd )"
  SOURCE="$(readlink "$SOURCE")"
  [[ $SOURCE != /* ]] && SOURCE="$DIR/$SOURCE"
done
SCRIPTS_DIR="$( cd -P "$( dirname "$SOURCE" )" >/dev/null 2>&1 && pwd )"

# Nyt tiedämme aina, että projektin juuri on yksi taso scripts-kansion yläpuolella
PROJECT_ROOT="$(cd "$SCRIPTS_DIR/.." && pwd)"
cd "$PROJECT_ROOT" || exit

rm -rf build/
