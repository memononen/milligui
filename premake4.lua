
local action = _ACTION or ""

solution "mgui"
	location ( "build" )
	configurations { "Debug", "Release" }
	platforms {"native", "x64", "x32"}
  
	project "example"
		kind "ConsoleApp"
		language "C"
		files { "example/example.c", "src/mgui.c", "lib/nanovg/*" }
		includedirs { "src", "lib/nanovg", "lib/nanosvg" }
		targetdir("build")
	 
		configuration { "linux" }
			 links { "X11","Xrandr", "rt", "GL", "GLU", "pthread" }

		configuration { "windows" }
			 links { "glu32","opengl32", "gdi32", "winmm", "user32" }

		configuration { "macosx" }
			links { "glfw3" }
			linkoptions { "-framework OpenGL", "-framework Cocoa", "-framework IOKit", "-framework CoreVideo" }

		configuration "Debug"
			defines { "DEBUG" }
			flags { "Symbols", "ExtraWarnings"}

		configuration "Release"
			defines { "NDEBUG" }
			flags { "Optimize", "ExtraWarnings"}    

	project "example2"
		kind "ConsoleApp"
		language "C"
		files { "example/example2.c", "src/milli.c", "lib/nanovg/*" }
		includedirs { "src", "lib/nanovg", "lib/nanosvg" }
		targetdir("build")
	 
		configuration { "linux" }
			 links { "X11","Xrandr", "rt", "GL", "GLU", "pthread" }

		configuration { "windows" }
			 links { "glu32","opengl32", "gdi32", "winmm", "user32" }

		configuration { "macosx" }
			links { "glfw3" }
			linkoptions { "-framework OpenGL", "-framework Cocoa", "-framework IOKit", "-framework CoreVideo" }

		configuration "Debug"
			defines { "DEBUG" }
			flags { "Symbols", "ExtraWarnings"}

		configuration "Release"
			defines { "NDEBUG" }
			flags { "Optimize", "ExtraWarnings"}    

	project "example3"
		kind "ConsoleApp"
		language "C"
		files { "example/example3.c", "src/milli2.c", "lib/nanovg/*" }
		includedirs { "src", "lib/nanovg", "lib/nanosvg" }
		targetdir("build")
	 
		configuration { "linux" }
			 links { "X11","Xrandr", "rt", "GL", "GLU", "pthread" }

		configuration { "windows" }
			 links { "glu32","opengl32", "gdi32", "winmm", "user32" }

		configuration { "macosx" }
			links { "glfw3" }
			linkoptions { "-framework OpenGL", "-framework Cocoa", "-framework IOKit", "-framework CoreVideo" }

		configuration "Debug"
			defines { "DEBUG" }
			flags { "Symbols", "ExtraWarnings"}

		configuration "Release"
			defines { "NDEBUG" }
			flags { "Optimize", "ExtraWarnings"}    
