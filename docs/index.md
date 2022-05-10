## CS284 Webpages

[Project Proposal](proposal.md)

[Milestone Report](report.md)

[Final Deliverables](final.md)

## Usage Guide

### Main functions
1. Edit the skeleton by adding, grabbing, scaling, and extruding the node spheres.   
2. Interpolate the skeleton and generate the mesh (press SPACE).  
3. Subdivide the mesh by pressing `d` or use the control bar.   
4. Rock and Roll using the `Shake it` button.  

### Software support
1. Save and load .balle file.
2. After mesh generation, export .obj file that could be useful in the Blender.
3. Press `0` to screen shot.
4. Default camera veiws are front, side and top views

### Using the GUI
Here are all the keybinds:

| Key | Usage  |
| --- | ---- | 
| `0` | Screen Shot|
| `1` | Set camera to front view |
| `3` | Set camera to side view |
| `7` | Set camera to top view |
| `Left click and drag (g)` | (grab) Move the selected node |
| `Right click and drag` or <br /> `s` + `Left click and drag ` | Scale the selected sphere |
| `e` |  Extrude the current node |
| `x` |  Delete the selected sphere |
| `r` | Reset all nodes to default sticky man 
| `n` |  Go to a sibling node (deprecated)|
| `p` |  Go to a parent node (deprecated)|
| `c` |  Go to a child node (deprecated)|
| `i` | Interpolate between the key spheres |
| `SPACE` | Interpolate the skeleton and generate the msh |
| `d` | Catmull-Clark subdivision |
| `w` | Show mesh wireframe |
| `ctrl + r` | Remesh |
| `ctrl + s` | Save |
| `ctrl + l` | Load |

Additionally, 
1. Left click and drag to rotate camera
2. Right click and drag to translate camera
3. Left click to apply changes (extrude, scale, grab)
4. Right click to reset changes, when in extrude, grab, or scale mode.

NOTE: The `1`, `3`, `7`, `e`, `g`, `s`, `x` keybinds are the default ones used in Blender for those purposes. They are kept the same for consistency.   
