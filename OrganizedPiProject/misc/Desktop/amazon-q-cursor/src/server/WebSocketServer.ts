import { WebSocketServer, WebSocket } from 'ws';
import { AmazonQService } from '../services/AmazonQService.js';
import { AmazonQRequest, WebSocketMessage } from '../types/index.js';

export class AmazonQWebSocketServer {
  private wss: WebSocketServer;
  private amazonQService: AmazonQService;
  private clients: Map<string, WebSocket> = new Map();

  constructor(port: number, amazonQService: AmazonQService) {
    this.amazonQService = amazonQService;
    this.wss = new WebSocketServer({ port });
    this.setupWebSocketServer();
  }

  private setupWebSocketServer(): void {
    this.wss.on('connection', (ws: WebSocket, request) => {
      const clientId = this.generateClientId();
      this.clients.set(clientId, ws);
      
      console.log(`Client connected: ${clientId}`);
      
      ws.on('message', async (data: Buffer) => {
        try {
          const message = JSON.parse(data.toString());
          await this.handleMessage(clientId, message);
        } catch (error) {
          console.error('Error handling message:', error);
          this.sendError(ws, 'Invalid message format');
        }
      });

      ws.on('close', () => {
        console.log(`Client disconnected: ${clientId}`);
        this.clients.delete(clientId);
      });

      ws.on('error', (error) => {
        console.error(`WebSocket error for client ${clientId}:`, error);
        this.clients.delete(clientId);
        // Send error notification to client if possible
        try {
          this.sendError(ws, 'Connection error occurred');
        } catch (sendError) {
          console.error('Failed to send error notification:', sendError);
        }
      });

      // Send welcome message
      this.sendMessage(ws, {
        type: 'status',
        data: { message: 'Connected to Amazon Q service', clientId },
        timestamp: new Date(),
      });
    });
  }

  private async handleMessage(clientId: string, message: any): Promise<void> {
    const ws = this.clients.get(clientId);
    if (!ws) return;

    try {
      switch (message.type) {
        case 'amazonq_request':
          await this.handleAmazonQRequest(ws, message.data);
          break;
        case 'ping':
          this.sendMessage(ws, {
            type: 'status',
            data: { message: 'pong' },
            timestamp: new Date(),
          });
          break;
        default:
          this.sendError(ws, `Unknown message type: ${message.type}`);
      }
    } catch (error) {
      console.error('Error handling message:', error);
      this.sendError(ws, error instanceof Error ? error.message : 'Unknown error');
    }
  }

  private async handleAmazonQRequest(ws: WebSocket, requestData: any): Promise<void> {
    try {
      const request: AmazonQRequest = {
        type: requestData.type,
        content: requestData.content,
        context: requestData.context,
        conversationId: requestData.conversationId,
      };

      // Send typing indicator
      this.sendMessage(ws, {
        type: 'typing',
        data: { isTyping: true },
        timestamp: new Date(),
      });

      const response = await this.amazonQService.processRequest(request);

      // Stop typing indicator
      this.sendMessage(ws, {
        type: 'typing',
        data: { isTyping: false },
        timestamp: new Date(),
      });

      // Send response
      this.sendMessage(ws, {
        type: 'message',
        data: response,
        timestamp: new Date(),
      });
    } catch (error) {
      console.error('Error processing Amazon Q request:', error);
      this.sendError(ws, error instanceof Error ? error.message : 'Failed to process request');
    }
  }

  private sendMessage(ws: WebSocket, message: WebSocketMessage): void {
    if (ws.readyState === WebSocket.OPEN) {
      ws.send(JSON.stringify(message));
    }
  }

  private sendError(ws: WebSocket, error: string): void {
    this.sendMessage(ws, {
      type: 'error',
      data: { error },
      timestamp: new Date(),
    });
  }

  private generateClientId(): string {
    return `client_${Date.now()}_${Math.random().toString(36).substr(2, 9)}`;
  }

  broadcast(message: WebSocketMessage): void {
    const messageStr = JSON.stringify(message);
    this.clients.forEach((ws) => {
      if (ws.readyState === WebSocket.OPEN) {
        ws.send(messageStr);
      }
    });
  }

  getConnectedClients(): number {
    return this.clients.size;
  }

  close(): void {
    // Close all client connections first
    this.clients.forEach((ws) => {
      if (ws.readyState === WebSocket.OPEN) {
        ws.close();
      }
    });
    this.clients.clear();
    
    // Close the WebSocket server
    this.wss.close();
  }
}
