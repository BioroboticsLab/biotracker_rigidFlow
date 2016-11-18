#Rigid flow tracker

This is a tracking module for the BioTracker framework. 

The tracker identifies point features on an object defined initially by the user. This object is then tracked by using a variant of the Hough transform to extract the most likely parameters of a rigid transform (rotation and translation) in a 2D plane. This tracker works with cluttered backgrounds but requires the object to be tracked to exhibit some texture (backlit scenes that produce homogeneous object appearance wouldn't work). This tracker was initially described to track honeybees (Landgraf & Rojas, 2007).

Required: 
* The Core library of the BioTracker framework: https://github.com/BioroboticsLab/biotracker_core
* The BioTracker GUI component: https://github.com/BioroboticsLab/biotracker_gui

Build instructions:
* see BioTracker's SampleTracker: https://github.com/BioroboticsLab/biotracker_sampletracker
