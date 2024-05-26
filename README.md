## Danger from the Deep - WW2 german submarine simulation

Authors:
See "CREDITS".

Web:
http://dangerdeep.sourceforge.net

License:
See "LICENSE".

Install:
See "INSTALL".

### Technical Notes

If you are using a setup with more than one monitor you may experience
problems.

SDL 1.2.11 seems to have problems when running the game on
multimonitor/multihead displays. If you're running the game on a
computer-setup which utilizes more than one screen, you may encounter
problems, for example your X-Session dropping to the Console. In that
case pressing ALT+F7 or ALT+CTRL+F7 might help.

This is no problem with the game, but a problem of the SDL-Framwork
we're using. The SDL-people claim that this issue should be sorted out
in version 2.0 of SDL.

On another note, your graphic-card must at least support OpenGL 1.5 and
even better, OpenGL 2.0 for decent graphics.
Your graphic-card MUST support HARDWARE ACCELERATION.

The game won't run on machines with a Cirrus Logic 4MB card.

We like pie.

### Gameplay

The game recommends a resolution of 1024x768 strongly.
It can be played with different resoultions, but will
look ugly. Just type dangerdeep --help and check for
resolution switching. Using a higher resolution is not as
bad as using a smaller, though.
General rule: what does not work that is just not
implemented yet ;-)

### Keys:

#### displays:

F1     Gauges
F2    Periscope
F3    UZO
F4    Bridge
F5    Map
F6    Torpedo loading control
F7    Damage control
F8    Log book
F9    Success records
F10    Free view (for now)

#### control:

Cursor keys    Rudder left/right/up/down
Cursor k.+shift    Turn faster
Return        Center rudders
1,2,3,4,5,6    Throttle slow,half,full,flank,stop,reverse
SHIFT 1...6    Fire torpedo in tube 1...6 (bow/stern tube number position
        relation depends on sub type: as first all bow tubes are
        enumerated, then the stern tubes)
Space        Select target
0 (zero)    Scope up/down
c        Crash dive (to 150 meters)
d        Snorkel depth
f        Snorkel up/down
h        Set heading to view
i        identify target
p        Periscope depth
s        Surface
t        Fire torpedo (automatic selection of bow/stern tubes)
g + SHIFT    Man/unman deck gun
g        Fire deck gun
v        Set view to heading
z        Zoom view (glasses, bridge and periscope view)

/+-     Zoom/unzoom map

, .        turn view
; :        turn view fast
F11/F12        time scale faster/slower
Pause        (Un)pause game
ESC        quit (for now)
PRINT        Take screenshot (uncompressed ppm, for now)

#### special for now:

Numpad 8,4,6,2,1,3    step forward/left/right/backward/up/down in freeview mode

### Closing words

This game is made as tactical simulation that tries to be
as realistic (historical and physical) as possible.
It is NOT meant to glorify war in any form.
Many thousands of people died in the Atlantic during 1939-1945
on both sides.


 ___ 


# readme.txt
Development readme
==================

The current state of the game has to be improved in many directions.
Feature branches have to be created for every change, to be as exclusive as
possible.

Benefit is that we can always start from a working version of the game and
stay there.

Certain changes or add-ons like generic_rudder or model_state can be
developed also in master branch, but better use a feature branch.


Rendering
---------

The complete rendering system of the game is outdated. Even worse, OpenGL
commands are all over the place, sometimes even SDL. There are a lot of
dependencies between libraries.
gpuinterfacetest branch tried to tackle this with a lot of changes, but is
not complete.
Rendering has to be separated from game state.
OpenGL commands have to be removed from rendering code and placed only in a
specific rendering class.
All rendering has to use only that class.
This means all interfaces/displays have to be changed.
The code progressed already quite far, but it is still a lot of work to
complete it.
Next steps:
the GUI (widget) has to be adapted and best would be to change it to a
vector style GUI, all coordinates no longer in fixed integer but in relative
screen coordinates. But this has to handle the aspect ratio of the screen,
as square elements should stay square. These changes could already be done
with current rendering, but we want to avoid double work by writing the
rendering of widgets with old and new render backend.
Font rendering is done in two steps and that is bad, we would rather use
SDL-ttf library to render TTF fonts at needed resultion on runtime.

The displays have also be improved to get rid of hand coded integer
coordinates and have a more generic handling which greatly simplifies the
code. What part of that can be added to current game without new rendering
code has to be checked.



Technical improvements
----------------------

Write generic sensor classes that can handle default stuff like
sweeping/rotating etc. and use them for radar, sonar, GHG, lookout, radio
report.
Implement sensor fusion code and tracking in the game. This makes the game
and the AI much more realistic and avoids storing pointers to sea_objects in
the game.
With that change handling of objects can be simplified, we no longer need
observable or object_ptr or special code to load/store sea_objects.
Special data of sensors like range of radar or specifics should be stored in
config files, not hardcoded in the game.

Sensor fusion is not so simple, as a categorization of contact in many
levels is needed, first level whether aircraft, ship, sub, smoke or so.
Second level would then be the rough size/category and then more the exact
type and so on. Lowest level could be individual object by name or so, but
that's not needed. Most probable position for merging is used.
An extrapolation of the course/position of the object should be done, here
we can add fuzzyness by experience of crew (for player and AI), which
greatly improves the game experience.
Measuring two positions with time difference also allows to measure course
and speed, so we don't have to handle measuring that by the sensor.
Larger distance of contact will give more fuzzyness of values, so we do not
store the exact position of the contact but with precision dependent on
distance.

This all is very good and cool from a theoretic point of view, but harder to
test and the players will not directly experience the benefits of it in the
game. We should mention that in a manual.

Also there needs to be some clever prediction for the AI if the course is
not straight, when ships are zig-zagging or better when hunting the player's
sub as that would certainly turn instead of running straight.
Here two or three escorts cooperating will give realistic results, as one
will detect the sub, report via radio and the other can hunt it. Problem is
that noise in the water (explosions) have to be simulated, so that sonar is
disturbed. Ships can't use their ASDIC when doing DC bombings. This has to
be handled.

Also a class to handle a generic rudder has to be moved out of the ship
class.

Model state has to be separated from model, so rendering can be split. There
is a model with the 3D data, a model_state for changes (angles/positions)
and a rendering class for the model. Here danger of correlation with
rendering changes.

Later also physical improvements are needed. The simulation of steering with
drag/drift is wrong. In reality it works that way: the rudder only turns the
sub/ship/aircraft and the difference between movement direction and
orientation causes a sidewards force, as the object shows more area in the
direction of movement after turning. This sidewards force changes the
position sidewards, which causes the wanted turning. So this is a two-step
process and actively used when manouvering boats. This explains the
"mystery" of physics that was mentioned before. It makes simulation a bit
more difficult, as the area from a certain direction is needed, but we
already have the data from optical detection (precomputed). It can be
obtained by rendering the vessels from any angle and counting pixels. This
could also be done on the gpu with N steps where every step looks up 2x2 or
4x4 pixels of the previous step and sums up. Resulting single float value is
the area. This would be much faster than measuring separately and the values
could be cached. But that would correlate with rendering changes.

All of that code should make use of data types describing physical units
(units.h) to have more declarative code and less bugs.
Here we have to check whether std::chrono can be used instead of user
defined duration, but the latter is better integrated.

See comments in sonar.h/cpp and sensor.h/cpp.
There must be only one class sonar_contact or maybe even only one class contact.
std::variant or std::optional for strength values or even noise band values.
The code in sonar.cpp especially for the GHG needs to be added as sensor code.
Merging contacts is not easy as sonar contacts only have strength or distance and
direction, so position is rather fuzzy. Merging two sonar contacts should have
higher distance limit as for lookout contacts, so rather merge.
Similarity of contacts (for sonar) is a bit different then, by comparing noise bands.
Maybe least sum of squares for comparison or so.

The visibility computation also includes light situation (time of day,
moonlight etc.) which should all be done in the lookout sensor. The
distinction between detection types in class game needs to be removed.
Whether a sensor is useable depends on its position, e.g. lookout sensor
only when sub is above zero. But we need a special sensor then for the
periscope, when player uses periscope and sees ships, the positions must be
added to contacts. Yes, that would be very realistic and cool.



Keep done changes
-----------------

Compare files between gpuinterfacetest branch and master, and do not lose
changes already done.



Terrain rendering
-----------------

Get much better ETOPO data with high precision and convert that to 2D
lines/polygons in a tree-like-structure, so we can generate any detail
within a tile. This makes map data much smaller. We only need to render the
coast lines and filled lines for the map.
Real 3D terrain rendering with geoclipmap is cool (and already working with
new gpu code) but in fact not really needed. Maybe in shallow waters for
outside view, but that can be done later, it's not really necessary.
So it could be deactivated, better show only the coastmaps.



Summary
=======

We need to list and prioritize changes
- Rudder changes
- Sensor changes
- Model state
- Phyiscal simulation changes
- Rendering changes Vector GUI
- Rendering changes general
- Terrain changes

Effort estimation.
Rudder changes could be a few hours, but involve testing.
Sensor changes are many more hours, with a lot more testing.
Model state few hours.
Physical simulation, many many hours.
Vector gui many many hours.
Complete rendering days/weeks.
Terrain changes also rather days.

Sensor changes would involve using new physical units, and although larger
change would be most interesting to start with.
Contact data is needed, like a 3d position. Sensor contacts have from a
position a direction and rough distance (strength) which can be converted to
an approximate position. So store per contact a fuzzyness factor, so larger
factors give larger radius for merging. Maybe better store rather  3d
position than direction and strength, but we may need strength to
compare/merge them. It means for sonar contacts store a noise band.
So we have basic type if known (sonar can not distinguish ships/subs
sometimes), which would be airplane, ship/sub, dc-explosions. Further
specification would then be plane/balloon, escort/war/merchant ship or sub.
Next level would then be rather exact ship class (and party, but given by
class). Maybe we need one level in between. So this makes 3 levels or 4.
Maybe needed for ships, large/small and next level rather tanker/freighter
and what kind of escort (destroyer, smaller). We can give these classes also
in the data files!
Start by writing design spec/documentation in sensor class, take that from
gpuinterfacetest branch. First create special branch.
This will lead to a lot of changes, no direct tracking of sea objects but
store rather contacts, and we need sensor fusion code.
So noise band needs to be converted to probable type of comparison. When
comparing noise bands this is easier. So a lot of possible combinations for
contacts.
Problem: what to show in 3d scene? The visible functions in class game are
used to determine what objects to draw. But with contacts that is not the
same, and contact position is fuzzy. So rendering should rather show real
objects, of course with culling. This will lead to the situation that
certain objects can be seen on screen but are not detected as contacts, but
most probably there will be contacts, so no problem.
Contact data also need an age, or last time point when detected, so they can
be discarded and that is also needed for course/speed computation.
So maybe start with the contact_data class.
Contacts that are too old should be removed. After merge time point is
latest.
If a contact is no longer seen, but course/speed known, the new position
could be extrapolated. Should be done anyway, to simplify merging.

So contact_data needs:
position (3d)
course/velocity (3d) - generic 3d vector is ok. Is zero on first contact,
	later changed on merge.
when_detected
Type of contact (including noise band for sonar contacts, with optional)
accuracy - needed for merging, how good are the values. Better when closer,
	or more often merged. How can we increase that over time, when we
	merge two values?

Merging:
Probability of type match, the finer the type the more similar it has to be.
Position comparison takes accuracy into account - higher -> smaller radius.




OLDER notes
-----------

matrix classes already include GL, so they can't be in base OR we link GL to
base!
Some other classes were also moved to media, because of that.
this is a nightmare.
and we need to add the OTHER subdirectories as include directories!!!
otherwise the files are not found
adding include_directories doesnt work as expected!
would using < > help instead of " " ?

most differences master<->gpuinterfacetest are pragma once.
can this be applied with clang tidy?
or compare files manually? There ARE changes like ai.h, so manual comparison
needed.
we can copy over that is the same in both branches.

get clang-tidy to work, e.g. std::string in ctor instead of const
std::String!

we can even copy over latest version of gpu_* from gpuinterfacetest branch
and get it to run with newer GL version - maybe add parameter to
system_interface

check if latest version of system_interface and input_event_handler is used.
Does that already work in master?

Does the game still work and run, even with sdl2?

replace all non-standard stuff like thread class etc. - maybe already done

Move code to subdirectories with matching libraries
What files to put to a library?
Add subdirs to include path! otherwise its complicated.
static libraries are ok

compare xml reading! already ok! but constructor is different!

maybe already move new mesh class to master. already exists, but is it ready?

we can already change storage/handling of objects.
And see ai.h, class game is no longer fed inside... why?

What are all the parts of the graphic that were changed in gpuinterfacetest
and need to make work again?
Working
- geoclipmap
- model rendering
Working partially
- water
- displays
- sky?
- moon?
Missing
- particle
- widget
- credits
- caustics?
- coastmap
- Test programs like geoclipmap




see further notes about development.
maybe GHG data or other sensor data like radar can be defined by config
files and not hardcoded. Then only some classes like passive_sonar_Sensor or
radar_sensor would be used, only basic classes and parameters come from
config files, IF specialities in behaviour would not be needed from code.
Question is even if that would be needed if that detail information can be
researched historically or if not the base class can handle it, like blind
spot for passive sonar etc.
With location of sensor one can even simulate towed sonars.





Get gpuinterfacetest branch back to main as highest priority!
what is missing there?
widget rendering for sure!
sync between master and this branch, master has different structure of
files! can be hard to compare. move files in branch accordingly?
THe interfaces have been improved in that branch, may collide with widget!

Apply clang-tidy to both branches to bring them on same level!
clang-format already applied on both.
Running clang-tidy doesnt work so far

clang-tidy:
configure cmake in build directory with
cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON ..
then run there
run-clang-tidy

compare all files of master and gpuinterfacetest, so only gpu changes are
left. For that introduce pragma once.
There are a lot of other differences, especially with modelstate etc., huge
work to synchronize.
most differences: widget, particle, water heights separate from rendering,
modelstate, water rendering broken

use std::function in gpu*

attach model_store to game or just a global variable? like the other cache
things? we don't need to remove that with game, as next game started can
reuse the models... although won't need them and rather use other models.
But we don't really care for cleanup, so we can keep a global variable,
and thus don't need parameters to ctor of sea_object then.

Build debug:
cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..

differences master<->gpu:
make_mesh completely different
bicetor too, but why

display code could be moved out of sea_object
particles as vector can be done already now
gunshell collision check better to class game, move out of gunshell class
rather generic collision check code in game, should already be there
widget class with better rendering
use generic_rudder
model_state also good idea
have we now object_index or id in model?
particle class itself looks ok, but there is no rendering

in general structural improvements in gpuinterfacetest branch that need to
get merged too, but separately. Taking working parts of gpuinterfacetest
branch to separate path and library does not work, as names collide. But
e.g. for class widget we need to save the changes somewhere and use widget
from master, only change rendering.
We need also test programs for more aspects, to test every converted part of
rendering.

development:
cd build
cmake -DCMAKE_BUILD_TYPE=Debug -DDFTD_BUILD_TOOLS=1 -DDFTD_BUILD_TESTS=1 ..


mymodelstate or using modelstates at all would be a helpful change.
modelstate has a pointer to model. And we need to ask it for a pointer to
create a gpu::model from.

add checks for int < 0 where it has been unsigned.

Vector GUI (widget)
===================

Separate coordinates (int/float) from vector rendering, as these are two
independent changes.
For rendering we don't use images as source for the themes but rather some
shaders or drawing code for frames, arrows and checkboxes. Themes would then
rather be code, as creating a list of triangles/quads/lines to render as
data format would be very cumbersome. So we can create a theme by shape and
by color. So far the themes have all more or less the same shapes and vary
only in color. We need drawing code for an arrow and the X-mark and for a
frame, and with that we can render everything. However frame width of
checkboxes differ from the window frame width, even when the rendering would
be the same.
For frames this would be four quads, maybe with additional lines in the
corners to simulate crisp edges. For the x-mark two quads, and for the arrow
some triangles.

actual bug: closing window inside game doesnt quit it.
maybe because of missing quit_exception

move highscorelist to some subdirectory, like core!
