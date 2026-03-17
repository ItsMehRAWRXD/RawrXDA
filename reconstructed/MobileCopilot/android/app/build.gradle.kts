plugins {
    id("com.android.application")
    kotlin("android")
}

android {
    namespace = "com.rawr.mobilecopilot"
    compileSdk = 34

    defaultConfig {
        applicationId = "com.rawr.mobilecopilot"
        minSdk = 26
        targetSdk = 34
        versionCode = 1
        versionName = "1.0"
    }

    buildTypes {
        release {
            isMinifyEnabled = false
        }
    }

    packagingOptions {
        resources {
            excludes += "/META-INF/{AL2.0,LGPL2.1}"
        }
    }

    buildFeatures { compose = true }
    composeOptions { kotlinCompilerExtensionVersion = "1.5.8" }
}

dependencies {
    implementation("androidx.core:core-ktx:1.12.0")
}

// Package models from E:/Models if present
import java.io.File

tasks.register<Copy>("packageModels") {
    val src = File("E:/Models")
    val dest = File("${project.projectDir}/src/main/assets/models")
    if (src.exists()) {
        from(src)
        into(dest)
    }
}

tasks.named("preBuild").configure {
    dependsOn("packageModels")
}
