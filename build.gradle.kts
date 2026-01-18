import org.gradle.language.jvm.tasks.ProcessResources

plugins {
    java
    `java-library`
}

group = "com.soda4fries.blackhole"
version = "1.0.0"

repositories {
    mavenCentral()
}

java {
    toolchain {
        languageVersion.set(JavaLanguageVersion.of(25))
    }
}

sourceSets {
    main {
        java {
            setSrcDirs(listOf("src/main/java"))
        }
        resources {
            setSrcDirs(listOf("src/main/resources"))
        }
    }
}

tasks.withType<JavaCompile> {
    options.encoding = "UTF-8"
}

tasks.register("getJextract") {
    description = "Download and setup jextract tool"
    group = "build"

    val jextractBin = file("jextract-25/bin/jextract")
    val jextractUrl = "https://download.java.net/java/early_access/jextract/25/2/openjdk-25-jextract+2-4_linux-x64_bin.tar.gz"

    // Only run if jextract doesn't exist
    onlyIf { !jextractBin.exists() }

    doLast {
        println("📥 Downloading jextract from ${jextractUrl}...")

        val tempFile = File.createTempFile("jextract", ".tar.gz")
        try {
            // Download
            ant.invokeMethod("get", mapOf(
                "src" to jextractUrl,
                "dest" to tempFile.absolutePath
            ))

            // Extract
            copy {
                from(tarTree(tempFile))
                into(projectDir)
            }

            // Make executable
            jextractBin.setExecutable(true)

            println("✅ jextract installed successfully")
        } finally {
            tempFile.delete()
        }
    }
}

tasks.register<Exec>("extractHeaders") {
    description = "Generate Java bindings from blackhole.h using jextract"
    group = "build"

    dependsOn("getJextract")

    val jextractBin = file("jextract-25/bin/jextract")
    val headerFile = file("shared/blackhole.h")
    val outputDir = file("src/main/java")
    val packageName = "com.soda4fries.blackhole"
    val headerClassName = "Blackhole_h"

    onlyIf { !file("src/main/java/com/soda4fries/blackhole/Blackhole_h.java").exists() }

    doFirst {
        outputDir.mkdirs()
        if (!jextractBin.exists()) {
            throw GradleException("jextract not found at ${jextractBin}. Run 'getJextract' task first.")
        }
        if (!headerFile.exists()) {
            throw GradleException("Header file not found: ${headerFile}")
        }
    }

    commandLine(
        jextractBin.absolutePath,
        "--output", outputDir.absolutePath,
        "--target-package", packageName,
        "--header-class-name", headerClassName,
        headerFile.absolutePath
    )

    doLast {
        println("✅ Java bindings generated in ${outputDir}")
    }
}

tasks.register<Exec>("buildNative") {
    description = "Build native library and eBPF programs using CMake"
    group = "build"

    val buildDir = file("build/resources/native/linux-x86_64")

    doFirst {
        buildDir.mkdirs()
    }

    workingDir = buildDir

    commandLine("bash", "-c", "cmake ../../../../native && make")
}

tasks.named("compileJava") {
    dependsOn("extractHeaders")
}

tasks.named<ProcessResources>("processResources") {
    dependsOn("buildNative")

    from("build/resources/native") {
        into("native")
    }
}

// Make standard build task do everything
tasks.named("build") {
    doLast {
        println("\n✅ Build complete!")
        println("JAR location: ${tasks.jar.get().archiveFile.get()}")
        }
}

// Alias for convenience
tasks.register("buildAll") {
    description = "Complete build: jextract, bindings, native code, and JAR (alias for 'build')"
    group = "build"
    dependsOn("build")
}

// Configure JAR to include native libraries
tasks.jar {
    manifest {
        attributes(
            "Implementation-Title" to "Blackhole IP Blocker",
            "Implementation-Version" to archiveVersion.get()
        )
    }

    from(configurations.runtimeClasspath.get().map { if (it.isDirectory) it else zipTree(it) })
    duplicatesStrategy = DuplicatesStrategy.EXCLUDE
}

// Task to run Java application
tasks.register<JavaExec>("runJava") {
    description = "Run Java application"
    group = "application"

    classpath = sourceSets["main"].runtimeClasspath
}

tasks.clean {
    delete("src/main/java/com/soda4fries/blackhole/Blackhole_h.java")
    delete("src/main/java/com/soda4fries/blackhole/Blackhole_h\$shared.java")
}

tasks.register("cleanAll") {
    description = "Clean all build artifacts including jextract"
    group = "build"

    dependsOn("clean")

    doLast {
        delete("jextract-25")
        println("🗑️  Removed jextract-25")
    }
}
