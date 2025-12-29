using System;

namespace WindowsForms.Services
{
    /// <summary>
    /// Информация об обновлении
    /// </summary>
    public class UpdateInfo
    {
        /// <summary>
        /// Доступно ли обновление
        /// </summary>
        public bool IsAvailable { get; set; }

        /// <summary>
        /// Текущая версия приложения
        /// </summary>
        public string CurrentVersion { get; set; }

        /// <summary>
        /// Последняя доступная версия
        /// </summary>
        public string LatestVersion { get; set; }

        /// <summary>
        /// URL для скачивания
        /// </summary>
        public string DownloadUrl { get; set; }

        /// <summary>
        /// Описание изменений
        /// </summary>
        public string ReleaseNotes { get; set; }

        /// <summary>
        /// Название релиза
        /// </summary>
        public string ReleaseName { get; set; }

        /// <summary>
        /// Дата публикации
        /// </summary>
        public DateTime PublishedAt { get; set; }

        /// <summary>
        /// Это пререлиз?
        /// </summary>
        public bool IsPrerelease { get; set; }

        /// <summary>
        /// Имя файла
        /// </summary>
        public string AssetName { get; set; }

        /// <summary>
        /// Размер файла в байтах
        /// </summary>
        public long FileSize { get; set; }
    }
}
