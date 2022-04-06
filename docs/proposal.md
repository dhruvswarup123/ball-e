## Ball-E: An Interactive Implementation of B-Mesh

### Summary

### Team Members
All members of the team are in CS284:

- Ce hao
- Dhruv Swarup
- Juntao Peng
- Yang Huang

### Problem Description
In the graphics industry, majority of the characters are created using a technique called sculpting. This involves literally sculpting a base mesh, as if it was a piece of clay. The problem with this method is that usually people need to start from scratch, and this can not only be very time consuming, but it can also create meshes with wrong proportions. 

Our proposal involves implementing a method for easily creating base meshes from only a small set of points. The original paper calls this method B-Mesh. There, the authors basically propose creating a set of points that are connected to each other, and spawning spheres of custom radii on each point. Then a "skin" is wrapped around this sphere. This is the core idea, but there are a lot of mathematical details that also need to be implemented. 

We plan to use the GUI from part 4, and add features that can help the user easily navigate the 3-D space, and spawn and scale those spheres (to be displayed as points). Ideally, the generated mesh would be shown to the user in real time. 

### Goals and Deliverables
Because there is a lot that can be done in this project, we propose our deliverables in two parts: the main goal, and stretch goals:

#### Main goals
As a part of the main goals, we want to build on the GUI given to us in Project 4. Our modified version should give the user control to add a point and then extrude points from it in any direction. It should also let the user scale the points to add a "thicker" mesh around that point. The GUI should give an option to enable realtime computation of the base mesh and display it as well. Naturally, that means that we will also implement the algorithm to calculate this base mesh. We will create images that show the original orientation of points, as well as the created base mesh around it. We will also submit videos that show the process of interacting with the GUI. 


Define how you will measure the quality / performance of your system (e.g. graphs showing speedup, or quantifying accuracy). It may not be possible to define precise target metrics at this time, but we encourage you to try.
What questions do you plan to answer with your analysis?

Here is a list of everything that we plan to deliver.  
1. A GUI for inserting and editing points.
2. A preview mode for the base mesh generation.


#### Stretch Goals
If we manage to complete the main goals, then we want to implement the following in order of increasing priority. 

- Add automatic rigging capabilities (based on the existing points). This involves creating bones, and parenting vertices to the bones based on weights, and then allowing the bones to be moved. 
- Expanding the rigging, by allowing user manipulation of the bones in our GUI. 
- Add support for animation creation by storing and playing back keyframes.
- Creating a feature to export model as an FBX file. This is an industry standard for storing animation files. 

### Schedule
In this section you should organize and plan the tasks and subtasks that your team will execute. Since presentations are ~4 weeks from the due-date of the proposal, you should include a set of tasks for every week.
Here is approximately how we plan to execute tasks:
1. 11-17th April: Create the whole GUI for point manipulation. Start the basic algorithm for B-Mesh. 
2. 18-24th April: Finish the first 2 parts of BMesh, Sphere interpolation, and skin wrapping. Work on the milestone
3. 25-1st May: Finish the Bmesh algorithm completely. 
4. 2-8th May: Bug fixes, final tweaks to the GUI, stretch goals

### Resources
List what resources, e.g. books, papers and/or online resources that are references for your project. List the computing platform, hardware and software resources that you will use for your project. You have a wide latitude here to use what you have access to, but be aware that you will have to support and trouble-shoot on your platform yourselves. If you are starting from an existing piece of code or system, describe and provide a pointer to it here.

1. [B-Mesh: A Fast Modeling System for Base Meshes
of 3D Articulated Shapes](http://citeseerx.ist.psu.edu/viewdoc/download?doi=10.1.1.357.7134&rep=rep1&type=pdf)

