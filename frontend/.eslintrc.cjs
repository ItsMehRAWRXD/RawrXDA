{
  "root": true,
  "env": {
    "browser": true,
    "es2021": true
  },
  "extends": [
    "eslint:recommended",
    "plugin:react/recommended",
    "plugin:react-hooks/recommended",
    "plugin:@typescript-eslint/recommended"
  ],
  "parser": "@typescript-eslint/parser",
  "parserOptions": {
    "ecmaVersion": "latest",
    "sourceType": "module",
    "ecmaFeatures": {
      "jsx": true
    }
  },
  "plugins": ["react-refresh"],
  "rules": {
    "react-refresh/only-export-components": "warn",
    "react/react-in-jsx-scope": "off",
    "@typescript-eslint/no-explicit-any": "warn"
  },
  "settings": {
    "react": {
      "version": "detect"
    }
  }
}
