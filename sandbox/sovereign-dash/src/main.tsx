import React from 'react'
import { hydrateRoot } from 'react-dom/client'
import App from './App'
import './styles.css'

hydrateRoot(document.getElementById('root')!,
  <React.StrictMode>
    <App />
  </React.StrictMode>,
)
