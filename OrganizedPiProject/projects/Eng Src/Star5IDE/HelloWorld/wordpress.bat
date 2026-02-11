@echo off
setlocal
echo Adding WordPress dev files...

REM -- .env
powershell -NoProfile -Command ^
  "$c=@'
WP_DB_NAME=star5ide
WP_DB_USER=star5ide
WP_DB_PASSWORD=supersecret
WP_TABLE_PREFIX=wp_
WP_URL=http://localhost:8080
'@; New-Item -ItemType Directory -Force -Path wordpress ^| Out-Null; Set-Content -Path wordpress\.env -Value $c -Encoding UTF8"

REM -- wp-cli.yml
powershell -NoProfile -Command ^
  "$c=@'
path: /var/www/html
color: true
'@; Set-Content -Path wordpress\wp-cli.yml -Value $c -Encoding UTF8"

REM -- docker-compose.yml
powershell -NoProfile -Command ^
  "$c=@'
services:
  db:
    image: mysql:8
    command: --default-authentication-plugin=mysql_native_password
    restart: unless-stopped
    environment:
      MYSQL_DATABASE: ${WP_DB_NAME}
      MYSQL_USER: ${WP_DB_USER}
      MYSQL_PASSWORD: ${WP_DB_PASSWORD}
      MYSQL_ROOT_PASSWORD: ${WP_DB_PASSWORD}
    volumes:
      - db_data:/var/lib/mysql

  wordpress:
    image: wordpress:php8.2-apache
    restart: unless-stopped
    ports:
      - "8080:80"
    environment:
      WORDPRESS_DB_HOST: db:3306
      WORDPRESS_DB_NAME: ${WP_DB_NAME}
      WORDPRESS_DB_USER: ${WP_DB_USER}
      WORDPRESS_DB_PASSWORD: ${WP_DB_PASSWORD}
      WORDPRESS_TABLE_PREFIX: ${WP_TABLE_PREFIX}
    volumes:
      - wp_data:/var/www/html

  wpcli:
    image: wordpress:cli-php8.2
    depends_on: [wordpress]
    user: xfs
    entrypoint: ["bash","-lc"]
    working_dir: /var/www/html
    volumes:
      - wp_data:/var/www/html
    environment:
      WP_CLI_CONFIG_PATH: /var/www/html/wp-cli.yml

  phpmyadmin:
    image: phpmyadmin:latest
    restart: unless-stopped
    ports: ["8081:80"]
    environment:
      PMA_HOST: db
      PMA_USER: ${WP_DB_USER}
      PMA_PASSWORD: ${WP_DB_PASSWORD}

volumes:
  db_data:
  wp_data:
'@; Set-Content -Path wordpress\docker-compose.yml -Value $c -Encoding UTF8"

REM -- .gitpod.yml append or create
if exist .gitpod.yml (
  powershell -NoProfile -Command ^
    "Add-Content -Path .gitpod.yml -Value @'
# --- WordPress (added by star5_add_wordpress.bat) ---
tasks:
  - name: WordPress
    init: docker compose -f wordpress/docker-compose.yml --env-file wordpress/.env up -d
    command: |
      echo ""Open the Preview for port 8080 (WordPress) and 8081 (phpMyAdmin).""
ports:
  - port: 8080
    onOpen: open-preview
  - port: 8081
    onOpen: ignore
'@"
) else (
  powershell -NoProfile -Command ^
    "Set-Content -Path .gitpod.yml -Value @'
tasks:
  - name: WordPress
    init: docker compose -f wordpress/docker-compose.yml --env-file wordpress/.env up -d
    command: |
      echo ""Open the Preview for port 8080 (WordPress) and 8081 (phpMyAdmin).""
ports:
  - port: 8080
    onOpen: open-preview
  - port: 8081
    onOpen: ignore
'@ -Encoding UTF8"
)

echo Done. Files created under wordpress\ and Gitpod config updated.
endlocal
