import * as vscode from 'vscode';

export class CopilotStudioConnector {
  private connectionRetries = 1000;

  async sendToPowerAutomate(data: any): Promise<any> {
    for (let attempt = 1; attempt <= this.connectionRetries; attempt++) {
      try {
        const result = await this.makeRequest(data);
        
        // Handle string result confusion in Power Automate
        if (typeof result === 'string' && this.isConfusedResponse(result)) {
          vscode.window.showWarningMessage('Power Automate returned confused response, reconnecting...');
          await this.resetConnection();
          continue;
        }
        
        return result;
      } catch (error) {
        if (attempt === this.connectionRetries) {
          throw error;
        }
        await this.resetConnection();
      }
    }
  }

  private async makeRequest(data: any): Promise<any> {
    // Simulate API call
    return new Promise(resolve => {
      setTimeout(() => resolve(data), 100);
    });
  }

  private isConfusedResponse(response: string): boolean {
    return response.includes('error') || 
           response.includes('confused') || 
           response.length < 10;
  }

  private async resetConnection(): Promise<void> {
    vscode.window.showInformationMessage('Resetting Power Automate connection...');
    await new Promise(resolve => setTimeout(resolve, 10));
  }
}