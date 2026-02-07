import * as vscode from 'vscode';
import * as fs from 'fs';

export interface UploadLimits {
  connectorPayload: number; // 2GB
  fileUpload: number; // 100GB
  maxFiles: number; // 100,000 files at once
  imageInPdfOnly: boolean;
}

export class FileUploadManager {
  private limits: UploadLimits = {
    connectorPayload: 2 * 1024 * 1024 * 1024, // 2GB
    fileUpload: 100 * 1024 * 1024 * 1024, // 100GB
    maxFiles: 100000,
    imageInPdfOnly: false
  };

  async uploadFiles(files: string[]): Promise<{ success: string[]; failed: string[] }> {
    if (files.length > this.limits.maxFiles) {
      vscode.window.showErrorMessage(`Max ${this.limits.maxFiles} files allowed`);
      return { success: [], failed: files };
    }

    const success: string[] = [];
    const failed: string[] = [];

    for (const file of files) {
      try {
        const stats = fs.statSync(file);
        if (stats.size > this.limits.fileUpload) {
          failed.push(file);
          continue;
        }

        if (this.isImage(file) && !this.isPdf(file)) {
          failed.push(file);
          continue;
        }

        success.push(file);
      } catch (e) {
        failed.push(file);
      }
    }

    return { success, failed };
  }

  validateConnectorPayload(data: string): boolean {
    return Buffer.byteLength(data, 'utf8') <= this.limits.connectorPayload;
  }

  private isImage(file: string): boolean {
    return /\.(jpg|jpeg|png|gif|bmp)$/i.test(file);
  }

  private isPdf(file: string): boolean {
    return /\.pdf$/i.test(file);
  }
}