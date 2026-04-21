# hyperspace

Hyperspace Vision API
--------------------------------------------------------

[vision-annotate.py]
- simple annotation tool to record json annotations for basic hyperspace plot recordings for one or two cameras.  the app places a circle on the screen, then the cross marker moves relative to it for movements relative to it of only your eyes.  we keep our natural head gaze centered on the circle, and move our eyes to the point, pressing space-bar when we have a fix.  every 10 iterations we move the
circle, and thus we have a comfortable way to record lots of samples as quickly as possible with ground-truth plotted as the pixel position on the screen.


