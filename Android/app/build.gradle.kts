plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
}

val versionMajor = 1
val versionMinor = 0
val versionPatch = 0
val versionBuild = 2

android {
    namespace = "com.example.lpstudio"
    compileSdk = 34

    defaultConfig {
        applicationId = "com.example.lpstudio"
        minSdk = 24
        targetSdk = 34
        versionCode = versionMajor * 10000000 + versionMinor * 100000 + versionPatch * 1000 + versionBuild
        versionName = "${versionMajor}.${versionMinor}.${versionPatch}.${versionBuild}"

        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"
        
        ndk {
            abiFilters.add("arm64-v8a")
			abiFilters.add("x86_64")
        }
        
        externalNativeBuild {
            cmake {
                cppFlags.add("-std=c++17")
                cppFlags.add("-fexceptions")
                cppFlags.add("-frtti")
                arguments.add("-DANDROID_STL=c++_static")
				arguments.add("-DANDROID_ARM_NEON=TRUE")
                arguments.add("-DANDROID_PLATFORM=android-24")
            }
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = false
            proguardFiles(
                getDefaultProguardFile("proguard-android-optimize.txt"),
                "proguard-rules.pro"
            )
        }
    }
    
    buildFeatures {
        viewBinding = true
    }
    
    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_1_8
        targetCompatibility = JavaVersion.VERSION_1_8
    }
    
    kotlinOptions {
        jvmTarget = "1.8"
    }
    
    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
            version = "3.22.1"
        }
    }
    
    sourceSets {
        getByName("main") {
            jniLibs.srcDirs("src/main/jniLibs")
        }
    }
    
    // WE ADD THIS BLOCK TO SOLVE THE 16KB PROBLEM
    packagingOptions {
        jniLibs {
            useLegacyPackaging = false
            // Явно исключите проблемные библиотеки если нужно
            excludes += listOf(
                "**/libc++_shared.so",
                "**/libprinter-jni.so"
            )
        }
        resources {
            excludes += "/META-INF/**"
        }
        // Явно указать pickFirst для конфликтующих библиотек
        pickFirsts += listOf(
            "**/libc++_shared.so",
            "**/libprinter-jni.so"
        )
    }
}

dependencies {
    // Material Design
    implementation("com.google.android.material:material:1.11.0")
    
    // AppCompat
    implementation("androidx.appcompat:appcompat:1.6.1")
    
    // ConstraintLayout
    implementation("androidx.constraintlayout:constraintlayout:2.1.4")
    
    //Kotlin extensions
    implementation("androidx.core:core-ktx:1.12.0")
    implementation("androidx.activity:activity-ktx:1.8.0")
    implementation("androidx.fragment:fragment-ktx:1.6.1")
	
	implementation("com.squareup.okhttp3:okhttp:4.12.0")
	implementation("androidx.lifecycle:lifecycle-runtime-ktx:2.8.7")
}
