from conans import ConanFile, CMake

class CameraImporterConan(ConanFile):
    name = "imaging"
    version = "1.0"
    settings = "os", "compiler", "build_type", "arch"
    exports = "*"
    requires = "lms_imaging/1.0@lms/stable","lms/2.0@lms/stable"
    generators = "cmake"

    def build(self):
        cmake = CMake(self.settings)
        self.run('cmake %s %s' % (self.conanfile_directory, cmake.command_line))
        self.run("cmake --build . %s" % cmake.build_config)

    def package(self):
        self.copy("*.h", dst="include",src="include")
        self.copy("*.lib", dst="lib", src="lib")
        self.copy("*.a", dst="lib", src="lib")
