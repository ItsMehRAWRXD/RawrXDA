# IRC mIRC IDE Bridge

This bridge lets your IDE join IRC and accept owner-only commands from your mIRC nick.

## Security model

- Only one nick is allowed: `controllerNick`.
- Commands must start with `!ide` and include your `sharedSecret`.
- Wrong secret is ignored.
- Bridge enforces cooldown + rate limit.
- Keep `sharedSecret` strong (8+ chars).

## Setup

1. Open Settings -> `IRC (mIRC)`.
2. Set:
- Host
- Port
- Channel
- Bot nick
- Your mIRC nick (`controllerNick`)
- Shared secret
3. Click `Save IRC config`.
4. Click `Start bridge`.
5. Click `Refresh status` and verify `running=true`.

Config is saved under Electron user data as `irc_bridge.json`.

## Command format

`!ide <secret> <command> [args]`

Examples:

- `!ide mySecret ping`
- `!ide mySecret help`
- `!ide mySecret workspace`
- `!ide mySecret tree`
- `!ide mySecret search TODO`
- `!ide mySecret tasks`
- `!ide mySecret status <taskId>`
- `!ide mySecret agent fix build errors in parser module`
- `!ide mySecret approve <taskId> yes`
- `!ide mySecret cancel <taskId>`
- `!ide mySecret rollback <taskId>`

## mIRC usage notes

- Send commands in the configured channel or via direct message to the bot nick.
- Your nick must exactly match `controllerNick`.
- If bot nick is taken, bridge auto-suffixes the nick.

## TLS notes

- Prefer TLS where available (often port 6697).
- `tlsInsecure` disables certificate verification and should be used only in lab environments.

## Troubleshooting

- `controllerNick is required`: set your mIRC nick in Settings and save.
- `sharedSecret is required`: set secret and save.
- `connect timeout`: verify host/port/TLS combo.
- `running=false`: read `lastLaunchError` and `recentLog` in `Refresh status` output.
