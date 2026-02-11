
@echo off
setlocal
if not exist wordpress\docker-compose.yml (
  echo Missing wordpress\docker-compose.yml. Run star5_add_wordpress.bat first.
  exit /b 1
)
cd wordpress
docker compose --env-file .env up -d
echo WordPress is starting... open http://localhost:8080
endlocal
