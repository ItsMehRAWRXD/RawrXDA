@echo off
setlocal
echo Capturing environment to _env.txt ...

(
  echo ==== VERSIONS ====
  node -v 2^>NUL
  npm -v 2^>NUL
  docker --version 2^>NUL
  git --version 2^>NUL

  echo.
  echo ==== GIT ====
  git rev-parse --show-toplevel 2^>NUL
  git status -s 2^>NUL
  git remote -v 2^>NUL
) > _env.txt

echo Done. See _env.txt
endlocal
