<h1 align=center>asciiquake</h1>
<p align=center>
   <img src=scr0.png width=300> <img src=scr1.png width=300>
   <br>
   <img src=scr2.png width=300> <img src=scr3.png width=300>
</p>

asciiquake is a small project where i dumbed down some of the stuff (such as sys and (the removal of) audio) and made it use the terminal as the output

note that this does not have every important key bound (on Linux so far)

## additional arguments when launching asciiquake:
- ### colors
   - no colors:

        `-nocolor`, `-none`, `-monochrome`
        
       `-clr=none`, `-color=none`, `-mode=none`,
       
       `-clr=0`, `-color=0`, `-mode=0`,
       
       `-clr=mono`, `-color=mono`, `-mode=mono`

   - 8 colors:

       `-8color`, `-ansi`, `-lowcolor`,

       `-clr=8`, `-color=8`, `-mode=8`,

       `-clr=ansi`, `-color=ansi`, `-mode=ansi`

   - 256 colors:

       `-notruecolor`, `-256color`, `-highcolor`,

       `-clr=256`, `-color=256`, `-mode=256`,

       `-clr=ext`, `-color=ext`, `-mode=ext`

   - truecolor:

       `-truecolor`, `-rgb`,

       `-clr=true`, `-color=true`, `-mode=true`,

       `-clr=rgb`, `-color=rgb`, `-mode=rgb`,

       `-clr=24`, `-color=24`, `-mode=24`

   - sixel:

       `-sixel`

       `-clr=sixel`, `-color=sixel`, `-mode=sixel`
- ### screen size
   - original size:
   
       `-scroriginalsize`, `-originalsize`
   
   - inbetween original and small size:
      <!-- isnt this misleading or something?? -->
       `-scrlarge`, `-highres`

## building:
```bash
cd WinQuake

cmake -B build
# or            cmake -B build -DCMAKE_TOOLCHAIN_FILE=lin_to_win.cmake`

make -j4
# or            cmake --build -j 4
# or            ninja -j4
# depending what build tool given by cmake (gnu make on linux, ninja on windows),
# you can likely look up how to build it with that tool or use the cmake one.
```

# Why?

i got bored, what else to ask.

# (john carmack's words from the 90's)


```
This is the complete source code for winquake, glquake, quakeworld, and 
glquakeworld.

The projects have been tested with visual C++ 6.0, but masm is also required 
to build the assembly language files.  It is possible to change a #define and 
build with only C code, but the software rendering versions lose almost half 
its speed.  The OpenGL versions will not be effected very much.  The 
gas2masm tool was created to allow us to use the same source for the dos, 
linux, and windows versions, but I don't really recommend anyone mess 
with the asm code.

The original dos version of Quake should also be buildable from these 
sources, but we didn't bother trying.

The code is all licensed under the terms of the GPL (gnu public license).  
You should read the entire license, but the gist of it is that you can do 
anything you want with the code, including sell your new version.  The catch 
is that if you distribute new binary versions, you are required to make the 
entire source code available for free to everyone.

Our previous code releases have been under licenses that preclude 
commercial exploitation, but have no clause forcing sharing of source code.  
There have been some unfortunate losses to the community as a result of 
mod teams keeping their sources closed (and sometimes losing them).  If 
you are going to publicly release modified versions of this code, you must 
also make source code available.  I would encourage teams to even go a step 
farther and investigate using public CVS servers for development where 
possible.

The primary intent of this release is for entertainment and educational 
purposes, but the GPL does allow commercial exploitation if you obey the 
full license.  If you want to do something commercial and you just can't bear 
to have your source changes released, we could still negotiate a separate 
license agreement (for $$$), but I would encourage you to just live with the 
GPL.

All of the Quake data files remain copyrighted and licensed under the 
original terms, so you cannot redistribute data from the original game, but if 
you do a true total conversion, you can create a standalone game based on 
this code.

I will see about having the license changed on the shareware episode of 
quake to allow it to be duplicated more freely (for linux distributions, for 
example), but I can't give a timeframe for it.  You can still download one of 
the original quake demos and use that data with the code, but there are 
restrictions on the redistribution of the demo data.

If you never actually bought a complete version of Quake, you might want 
to rummage around in a local software bargain bin for one of the originals, 
or perhaps find a copy of the "Quake: the offering" boxed set with both 
mission packs.

Thanks to Dave "Zoid" Kirsh and Robert Duffy for doing the grunt work of 
building this release.

John Carmack
Id Software


```
