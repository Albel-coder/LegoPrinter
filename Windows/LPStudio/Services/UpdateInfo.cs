using System;

namespace LPStudio.Services
{
    /// <summary>
    /// Update information
    /// </summary>
    public class UpdateInfo
    {
        /// <summary>
        /// Is there an update available?
        /// </summary>
        public bool IsAvailable { get; set; }

        /// <summary>
        /// Current version of the application
        /// </summary>
        public string CurrentVersion { get; set; }

        /// <summary>
        /// Latest version available
        /// </summary>
        public string LatestVersion { get; set; }

        /// <summary>
        /// Download URL
        /// </summary>
        public string DownloadUrl { get; set; }

        /// <summary>
        /// Description of changes
        /// </summary>
        public string ReleaseNotes { get; set; }

        /// <summary>
        /// Release name
        /// </summary>
        public string ReleaseName { get; set; }

        /// <summary>
        /// Publication date
        /// </summary>
        public DateTime PublishedAt { get; set; }

        /// <summary>
        /// Is this a prerelease?
        /// </summary>
        public bool IsPrerelease { get; set; }

        /// <summary>
        /// File name
        /// </summary>
        public string AssetName { get; set; }

        /// <summary>
        /// File size in bytes
        /// </summary>
        public long FileSize { get; set; }
    }
}
