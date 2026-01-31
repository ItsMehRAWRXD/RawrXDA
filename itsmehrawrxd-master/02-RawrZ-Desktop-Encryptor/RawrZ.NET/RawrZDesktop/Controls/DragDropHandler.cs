using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Windows.Forms;

namespace RawrZDesktop.Controls
{
    public class DragDropHandler
    {
        private readonly Form _parentForm;
        private readonly List<string> _supportedExtensions;
        private readonly Action<List<string>> _onFilesDropped;

        public DragDropHandler(Form parentForm, Action<List<string>> onFilesDropped)
        {
            _parentForm = parentForm;
            _onFilesDropped = onFilesDropped;
            _supportedExtensions = new List<string>
            {
                ".txt", ".doc", ".docx", ".pdf", ".xls", ".xlsx", ".ppt", ".pptx",
                ".jpg", ".jpeg", ".png", ".gif", ".bmp", ".tiff", ".svg",
                ".mp3", ".mp4", ".avi", ".mkv", ".mov", ".wav", ".flac",
                ".zip", ".rar", ".7z", ".tar", ".gz",
                ".exe", ".dll", ".msi", ".deb", ".rpm",
                ".cs", ".cpp", ".c", ".h", ".java", ".py", ".js", ".html", ".css",
                ".json", ".xml", ".sql", ".db", ".sqlite",
                ".bin", ".dat", ".log", ".cfg", ".ini"
            };

            SetupDragDrop();
        }

        private void SetupDragDrop()
        {
            _parentForm.AllowDrop = true;
            _parentForm.DragEnter += OnDragEnter;
            _parentForm.DragDrop += OnDragDrop;
            _parentForm.DragOver += OnDragOver;
        }

        private void OnDragEnter(object? sender, DragEventArgs e)
        {
            if (e.Data?.GetDataPresent(DataFormats.FileDrop) == true)
            {
                var files = (string[])e.Data.GetData(DataFormats.FileDrop);
                if (files != null && files.Any(IsSupportedFile))
                {
                    e.Effect = DragDropEffects.Copy;
                }
                else
                {
                    e.Effect = DragDropEffects.None;
                }
            }
            else
            {
                e.Effect = DragDropEffects.None;
            }
        }

        private void OnDragOver(object? sender, DragEventArgs e)
        {
            if (e.Data?.GetDataPresent(DataFormats.FileDrop) == true)
            {
                var files = (string[])e.Data.GetData(DataFormats.FileDrop);
                if (files != null && files.Any(IsSupportedFile))
                {
                    e.Effect = DragDropEffects.Copy;
                }
                else
                {
                    e.Effect = DragDropEffects.None;
                }
            }
            else
            {
                e.Effect = DragDropEffects.None;
            }
        }

        private void OnDragDrop(object? sender, DragEventArgs e)
        {
            if (e.Data?.GetDataPresent(DataFormats.FileDrop) == true)
            {
                var files = (string[])e.Data.GetData(DataFormats.FileDrop);
                if (files != null)
                {
                    var supportedFiles = files.Where(IsSupportedFile).ToList();
                    if (supportedFiles.Any())
                    {
                        _onFilesDropped(supportedFiles);
                    }
                }
            }
        }

        private bool IsSupportedFile(string filePath)
        {
            if (string.IsNullOrEmpty(filePath))
                return false;

            try
            {
                var extension = Path.GetExtension(filePath).ToLowerInvariant();
                return _supportedExtensions.Contains(extension) || Directory.Exists(filePath);
            }
            catch
            {
                return false;
            }
        }

        public void AddSupportedExtension(string extension)
        {
            if (!string.IsNullOrEmpty(extension) && !_supportedExtensions.Contains(extension.ToLowerInvariant()))
            {
                _supportedExtensions.Add(extension.ToLowerInvariant());
            }
        }

        public void RemoveSupportedExtension(string extension)
        {
            _supportedExtensions.Remove(extension.ToLowerInvariant());
        }

        public List<string> GetSupportedExtensions()
        {
            return new List<string>(_supportedExtensions);
        }
    }
}
