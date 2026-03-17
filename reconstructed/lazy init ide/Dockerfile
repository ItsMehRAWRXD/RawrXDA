FROM mcr.microsoft.com/powershell:lts-alpine-3.18
WORKDIR /app
COPY . /app
RUN mkdir -p /app/logs /app/config
ENV POWERSHELL_TELEMETRY_OPTOUT=1
ENV RAWRXD_ENV=production
CMD ["pwsh", "-NoLogo", "-ExecutionPolicy", "Bypass", "-File", "RawrXD.ps1"]
HEALTHCHECK --interval=30s --timeout=5s --start-period=10s --retries=3 CMD pwsh -c "Test-Path /app/logs/RawrXD.log"
VOLUME ["/app/logs", "/app/config"]
