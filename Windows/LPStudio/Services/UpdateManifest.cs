using Newtonsoft.Json;
using System;
using System.Collections.Generic;
using System.Runtime.Serialization;

namespace LPStudio.Services
{
    /// <summary>
    /// Манифест обновления (update.json)
    /// </summary>
    public class UpdateManifest
    {
        [JsonProperty("productName")]
        public string ProductName { get; set; }

        [JsonProperty("productVersion")]
        public string ProductVersion { get; set; } // 3 числа для пользователя (GitHub Release)

        [JsonProperty("releaseNotes")]
        public string ReleaseNotes { get; set; }

        [JsonProperty("changelogUrl")]
        public string ChangelogUrl { get; set; }

        [JsonProperty("platforms")]
        public Dictionary<string, PlatformInfo> Platforms { get; set; }
    }

    /// <summary>
    /// Информация о платформе в манифесте
    /// </summary>
    public class PlatformInfo
    {
        [JsonProperty("version")]
        private string _version { get; set; } // 4 числа в JSON

        [JsonIgnore]
        public string AvailableVersion { get; private set; } // 3 числа

        [JsonIgnore]
        public int Build { get; private set; }

        [JsonProperty("url")]
        public string Url { get; set; }

        [JsonProperty("minVersion")]
        public string MinVersion { get; set; } // Минимальная версия в формате 3 чисел

        [JsonProperty("releaseDate")]
        public DateTime ReleaseDate { get; set; }

        [JsonProperty("installerType")]
        public string InstallerType { get; set; }

        [JsonProperty("fileSize")]
        public long FileSize { get; set; }

        [JsonProperty("sha256")]
        public string Sha256 { get; set; }

        [JsonProperty("signature")]
        public string Signature { get; set; }

        [JsonProperty("changelog")]
        public string Changelog { get; set; }

        [JsonProperty("isRequired")]
        public bool IsRequired { get; set; }

        [JsonProperty("isDeprecated")]
        public bool IsDeprecated { get; set; }

        [JsonProperty("isCritical")]
        public bool IsCritical { get; set; }

        /// <summary>
        /// Проверяет доступность обновления
        /// </summary>
        public UpdateCheckResult CheckForUpdate(string currentVersion, int currentBuild)
        {
            var isUpdateAvailable = UpdateHelper.IsUpdateAvailable(
                currentVersion, currentBuild,
                AvailableVersion, Build
            );

            var isCompatible = string.IsNullOrEmpty(MinVersion) ||
                UpdateHelper.IsMinVersionRequired(
                    currentVersion, currentBuild,
                    MinVersion, 0 // Минимальная версия всегда имеет build = 0
                );

            return new UpdateCheckResult
            {
                IsUpdateAvailable = isUpdateAvailable,
                IsRequired = IsRequired,
                IsDeprecated = IsDeprecated,
                IsCritical = IsCritical,
                IsCompatible = isCompatible
            };
        }

        [OnDeserialized]
        private void OnDeserialized(StreamingContext context)
        {
            // Разбираем 4-числовую версию из JSON на 3-числовую версию и сборку
            var cleanVersion = UpdateHelper.CleanVersion(_version);
            var parts = cleanVersion.Split('.');

            if (parts.Length >= 3)
            {
                AvailableVersion = $"{parts[0]}.{parts[1]}.{parts[2]}";
            }
            else
            {
                AvailableVersion = cleanVersion;
            }

            if (parts.Length >= 4 && int.TryParse(parts[3], out int build))
            {
                Build = build;
            }
            else
            {
                Build = 0;
            }

            // Нормализуем минимальную версию до 3 чисел
            if (!string.IsNullOrEmpty(MinVersion))
            {
                MinVersion = UpdateHelper.CleanVersion(MinVersion);
                var minParts = MinVersion.Split('.');
                if (minParts.Length >= 3)
                {
                    MinVersion = $"{minParts[0]}.{minParts[1]}.{minParts[2]}";
                }
            }
        }
    }

    /// <summary>
    /// Результат проверки обновления
    /// </summary>
    public class UpdateCheckResult
    {
        public bool IsUpdateAvailable { get; set; }
        public bool IsRequired { get; set; }
        public bool IsDeprecated { get; set; }
        public bool IsCritical { get; set; }
        public bool IsCompatible { get; set; }
    }

    /// <summary>
    /// Информация об обновлении для UI
    /// </summary>
    public class UpdateInfo
    {
        public bool IsAvailable { get; set; }
        public bool IsRequired { get; set; }
        public bool IsDeprecated { get; set; }
        public bool IsCompatible { get; set; }
        public bool IsCritical { get; set; }

        public string CurrentVersion { get; set; } // 3 числа
        public int CurrentBuild { get; set; }
        public string LatestVersion { get; set; } // 3 числа
        public int LatestBuild { get; set; }
        public string MinRequiredVersion { get; set; } // 3 числа

        public string DownloadUrl { get; set; }
        public string ReleaseNotes { get; set; }
        public string GeneralReleaseNotes { get; set; }
        public string ProductName { get; set; }
        public string ProductVersion { get; set; } // 3 числа
        public DateTime PublishedAt { get; set; }
        public string InstallerType { get; set; }
        public long FileSize { get; set; }
        public string Checksum { get; set; }
        public string Signature { get; set; }
        public string ChangelogUrl { get; set; }
        public string AssetName { get; set; }
        public string ReleaseName { get; set; }
    }
}
