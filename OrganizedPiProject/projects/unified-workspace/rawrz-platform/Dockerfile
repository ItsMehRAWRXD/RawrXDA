# Generated for true
FROM node:20-slim

WORKDIR /app

RUN apt-get update && apt-get install -y --no-install-recommends dumb-init \
 && rm -rf /var/lib/apt/lists/*

COPY package*.json ./
RUN npm ci --only=production

COPY . .

RUN mkdir -p /app/uploads && chown -R node:node /app/uploads

USER node

EXPOSE 8080
ENV NODE_ENV=production

CMD ["dumb-init", "node", "server.js"]
