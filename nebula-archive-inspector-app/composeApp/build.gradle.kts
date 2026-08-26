import org.jetbrains.compose.desktop.application.dsl.TargetFormat
import org.jetbrains.kotlin.gradle.dsl.JvmTarget

// The one place the version is written down. `versionName` keeps the codename the
// changelog refers to; installers accept digits only, so they get the numeric prefix.
val appVersionName = "0.14.0-performance-causes-and-diff"
val appVersion = appVersionName.substringBefore('-')

// jpackage refuses a leading zero in a macOS bundle version, so the .dmg carries the
// same minor and patch under major 1. Every other format uses `appVersion` unchanged.
val macAppVersion = "1." + appVersion.substringAfter('.')

plugins {
    alias(libs.plugins.kotlinMultiplatform)
    alias(libs.plugins.androidApplication)
    alias(libs.plugins.composeMultiplatform)
    alias(libs.plugins.composeCompiler)
}

kotlin {
    androidTarget {
        compilerOptions { jvmTarget.set(JvmTarget.JVM_17) }
    }
    jvm("desktop") {
        compilerOptions { jvmTarget.set(JvmTarget.JVM_17) }
    }

    sourceSets {
        val commonMain by getting {
            dependencies {
                implementation(compose.runtime)
                implementation(compose.foundation)
                implementation(compose.material3)
                implementation(compose.ui)
                implementation(libs.kotlinx.coroutines.core)
            }
        }
        val commonTest by getting {
            dependencies {
                implementation(kotlin("test"))
            }
        }

        // Android and desktop are both JVM platforms: ZIP extraction and disk access are written once here.
        val jvmShared by creating {
            dependsOn(commonMain)
        }

        val androidMain by getting {
            dependsOn(jvmShared)
            dependencies {
                implementation(libs.androidx.activity.compose)
                implementation(libs.kotlinx.coroutines.android)
            }
        }
        val desktopMain by getting {
            dependsOn(jvmShared)
            dependencies {
                implementation(compose.desktop.currentOs)
                implementation(libs.kotlinx.coroutines.swing)
            }
        }
        val desktopTest by getting {
            dependencies {
                implementation(kotlin("test"))
            }
        }
    }
}

android {
    namespace = "app.nebula.archive"
    compileSdk = 35

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

    signingConfigs {
        getByName("debug") {
            storeFile = rootProject.file("signing/nebula-debug.keystore")
            storePassword = "android"
            keyAlias = "androiddebugkey"
            keyPassword = "android"
        }
    }

    defaultConfig {
        applicationId = "app.nebula.archive"
        minSdk = 26
        targetSdk = 35
        versionCode = 14
        versionName = appVersionName
    }
}

compose.desktop {
    application {
        mainClass = "app.nebula.archive.DesktopMainKt"

        nativeDistributions {
            targetFormats(TargetFormat.Deb, TargetFormat.Msi, TargetFormat.Dmg)
            packageName = "NebulaArchiveInspector"
            packageVersion = appVersion
            description = "Inspect the contents of a web archive: files, source, assets and diffs."
            vendor = "Nebula"

            linux {
                // Debian package names are lowercase and hyphenated; the menu entry is not.
                packageName = "nebula-archive-inspector"
                debMaintainer = "edikdr@users.noreply.github.com"
                menuGroup = "Development"
            }

            windows {
                // Fixed once and never changed: Windows matches installers by this UUID to
                // tell an upgrade from a second, parallel installation.
                upgradeUuid = "9f3c1d84-5e27-4a6b-8f10-2c7b6d94ae53"
                menuGroup = "Nebula"
                perUserInstall = true
            }

            macOS {
                packageVersion = macAppVersion
                bundleID = "app.nebula.archive"
            }
        }
    }
}
