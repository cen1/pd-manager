from conan import ConanFile
from conan.tools.cmake import CMakeToolchain

class Quazip(ConanFile):
	settings = "os", "compiler", "build_type", "arch"
	generators = "CMakeDeps"

	requires = (
		"libmysqlclient/8.1.0",
		"boost/1.86.0",
		"zlib/1.3.1",
	)

	def configure(self):
		self.options["boost"].header_only = False
		self.options["boost"].without_system = False
		self.options["boost"].without_filesystem = False
		self.options["boost"].without_chrono = False
		self.options["boost"].without_thread = False
		self.options["boost"].without_date_time = False
		self.options["boost"].without_regex = False
		self.options["boost"].without_atomic = False
		self.options["boost"].without_container = False
		self.options["boost"].without_context = False
		self.options["boost"].without_contract = True
		self.options["boost"].without_coroutine = True
		self.options["boost"].without_exception = False
		self.options["boost"].without_graph = True
		self.options["boost"].without_iostreams = True
		self.options["boost"].without_json = True
		self.options["boost"].without_locale = True
		self.options["boost"].without_log = True
		self.options["boost"].without_math = True
		self.options["boost"].without_mpi = True
		self.options["boost"].without_nowide = True
		self.options["boost"].without_program_options = True
		self.options["boost"].without_python = True
		self.options["boost"].without_random = True
		self.options["boost"].without_serialization = True
		self.options["boost"].without_stacktrace = True
		self.options["boost"].without_test = True
		self.options["boost"].without_timer = True
		self.options["boost"].without_type_erasure = True
		self.options["boost"].without_url = True
		self.options["boost"].without_wave = True

	def generate(self):
		tc = CMakeToolchain(self)
		tc.user_presets_path = False
		tc.generate()
