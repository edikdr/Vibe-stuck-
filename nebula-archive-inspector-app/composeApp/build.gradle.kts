import org.jetbrains.kotlin.gradle.dsl.JvmTarget

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
        versionName = "0.14.0-performance-causes-and-diff"
    }
}

compose.desktop {
    application {
        mainClass = "app.nebula.archive.DesktopMainKt"
    }
}
