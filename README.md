# TPDPHook-Extended
This is a fork of [TPDPHook](https://github.com/php42/TPDPHook) used by [SoD -Extended- Mod](http://en.tpdpwiki.net/wiki/Mod:Shard_of_Dreams_-_Extended_-).  
Unlike it's predecessor, this one is not a pure DLL proxy, it requires the modified game executable that ships with extended.  
[Xdelta](http://xdelta.org/) patches to create this modified exe are provided in the [patches](patches) folder.

Differences from the official Extended mod release:  
- A certain *easter egg* has been removed.
- Internal configuration file has been removed in favor of the external one.

## Usage
Grab a release zip from the [releases](https://github.com/php42/TPDPHook-Extended/releases/latest) page and extract dxgi.dll and TPDPHook.ini into the game folder.  
TPDPHook.ini contains various settings that can be configured to your liking. The one provided in release packages
is heavily commented with examples.

You will be required to launch the game with one of the modified executables from the [patches](patches) folder or from the official extended mod release.

## Programming Guide
API for accessing game code/data: [tpdp_data.h](src/tpdp/tpdp_data.h)  
Skill implementations: [custom_skills.cpp](src/tpdp/custom_skills.cpp)  
Switch-in abilities: [custom_abilities.cpp](src/tpdp/custom_abilities.cpp)  
Memory structures: [mem_structs.h](src/tpdp/mem_structs.h)  
Everything else (mostly): [misc_hacks.cpp](src/tpdp/misc_hacks.cpp)

Skill/Ability/Item callback functions are called once every frame and generally use some sort of state machine
implementation to keep track of their state between calls.  
They are expected to return `1` or `true` to signal that they want to be called again next frame, and `0` or `false`
to signal that they're done and should not be called again.  
These callbacks are usually called by a "dispatch" function hooked into the original call site to override or
extend the functionality of the original dispatcher.  
```cpp
// callback for a new item
bool my_item()
{
    static auto state = 0; // static vars persist between calls

    switch(state)
    {
    case 0:
        // step 1
        state = 1; // go to next step
        return true; // keep going

    case 1:
        // step 2
        state = 0 // reset state before we end
        return false; // we're done
    }
}

// add our callback to the dispatch function
bool item_dispatch()
{
    if(activation_condition && my_item())
        return true;
}
```

## Compiling
Prerequisites:  
[CMake](https://cmake.org/), and Visual Studio 2022.

Cloning the repo:  
Make sure to clone with submodules: `git clone --recurse-submodules https://github.com/php42/TPDPHook-Extended.git`  
If you already cloned without submodules, you can get them like so: `git submodule update --init --recursive`

Configuration:  
For easy setup, you can just run [configure.bat](configure.bat) in the root of the source tree 
(requires CMake to be installed with the "add to system PATH" option).  
This will create a subfolder called `build` which contains the Visual Studio project files.

Notes:
`configure.bat` assumes VS 2022, however any version of VS supporting C++17 may be used if you plan to run CMake yourself.  
The following constraints apply regardless of compiler used:
- Must target 32-bit x86
- Must be built with SSE2 enabled (to avoid clobbering x87 state)
- Must support inline assembly and `__declspec(naked)`
