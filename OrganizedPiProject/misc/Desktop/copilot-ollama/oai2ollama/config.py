from pydantic import Field, HttpUrl
from pydantic_settings import BaseSettings


class Settings(BaseSettings):
    model_config = {
        "cli_parse_args": True,
        "cli_kebab_case": True,
        "cli_ignore_unknown_args": True,
        "env_prefix": "OPENAI_",
        "env_file": ".env",
        "extra": "ignore",
    }

    api_key: str = Field(description="API key for authentication")
    base_url: HttpUrl = Field(description="Base URL for the OpenAI-compatible API")


env = Settings()  # type: ignore

print(env)
