---
title: 'SeismicMesh: Triangular meshing for seismology'
tags:
  - Python
  - Seismology
  - Mesh generation
  - DistMesh
  - Full Waveform Inversion
  - Parallel mesh generation
authors:
  - name: Keith Jared Roberts
    affiliation: 1
  - name: Rafael dos Santos Gioria
    affiliation: 1
  - name: William J. Pringle
    affiliation: 2
affiliations:
 - name: Escola Politécnica da Universidade de São Paulo
   index: 1
 - name: Argonne National Laboratory
   index: 2
date: 11 August 2020
bibliography: paper.bib

---

# Summary

`SeismicMesh` is a Python package for generating quality two and three dimensional triangular and tetrahedral
meshes from seismic velocity models using signed distance functions to define geometry. C++ code is wrapped with Python for high performance without losing flexibility or ease-of-use. Mesh generation is accomplished through modifications to a smoothing-based mesh generation algorithm known as `DistMesh` [@doi:10.1137/S0036144503429121]. The program calls the Computational Geometry Algorithms Library [@cgal:hs-chdt3-20a] to robustly and efficiently produce Delaunay triangulations. Computationally expensive operations are parallelized through an implementation of a parallel Delaunay triangulation algorithm [@peterka2014high].

# Background

Generating a high-geometric quality graded mesh for a geophysical domain represents a modern
challenge for sophisticated geophysical simulation workflows. In these applications,
a geophysical domain is discretized typically with triangles/tetrahedral elements that vary widely in size around features of interest. In geophysics, these meshes are commonly used with the Finite Element
Method (FEM) to solve partial differential equations that model acoustic or elastic waves. Geophysical exploration studies use these meshes to solve inverse problems for seismic velocity model building such as Full Waveform Inversion (FWI) [@doi:10.1190/1.1441754; @virieux2009overview]
and Reverse Time Migration (RTM) [@10.1093/gji/ggv380]. Many forward and adjoint simulations are computed in these inversion problems, which motivates a mesh that needs to efficiently discretize the domain and be accurate to simulate with.

# Rationale

Self-contained automatic meshing workflows that incorporate geophysical data to generate unstructured meshes have been successful in several geophysical domains such as coastal ocean modeling `OceanMesh2D`[@roberts2019oceanmesh2d], `GMT` and `Terreno`, [@gorman2008systematic] and reservoir modeling `MeshIT` [@cacace2015meshit] to name a few. The overall motivation behind these tools is that, besides providing a tool to generate a high-quality mesh, there are numerous application-specific aspects to consider in the design of the mesh. For example, the distribution of elements must accurately represent physical features in the domain such as faults and the location of wells. At the same time, mesh resolution must respect the Courant-Friedrichs-Lewey condition to ensure numerical stability when used in simulations. In applications such as FWI and RTM, material discontinuities in the interior of the domain need to be resolved to ensure reflection and refraction of waves are modeled accurately as compared to observed data. Domain extensions are also needed to be built into the mesh, which absorb outgoing waves and thus minimize artificial boundary reflections. In domains with irregular free surface boundaries such as land-based seismological surveys, explicit geometry data of the boundary surface may not exist requiring the use of external programs to create it and then to mesh it. These domain-specific aspects motivate the development of a self-contained mesh generation program.

# Software architecture

A schematic of `SeismicMesh` is shown in \autoref{fig:workflow}. The core functionality is as follows:

 1. The creation of 2D/3D graded mesh size functions defined on regular Cartesian grids. These mesh sizing functions contain mesh resolution variations that conform to the variations from inputted seismic velocity model data and are distributed according to several heuristics. Mesh size function grading is accomplished using [@persson2006mesh].

 2. The generation of potentially large (> 10 million cells) high-geometric quality triangular or tetrahedral meshes in either serial or using distributed parallelism.

 3. An implementation of a 3D degenerate (sliver) tetrahedral element removal technique [@tournois2009perturbing] to bound a mesh quality metric while preserving the domain structure.


![A schematic of `SeismicMesh`. On the right hand side, a P-wave seismic velocity in the Canadian Rockies is shown [@gray1995migration]  \label{fig:workflow}](Workflow.jpg)

Similar to other meshing programs such as `gmsh` [@doi:10.1002/nme.2579], `tetgen` [@si2015tetgen] and `mmg` [@mmg], `SeismicMesh` provides both generation and improvement of meshes through a scripting-based application programming interface. A point of difference from the aforementioned software programs is a convenience class that can be used to generate graded mesh sizing functions from seismic velocity models. Using this capability, mesh resolution is distributed to resolve material variations within the interior of the domain while at the same time statisfying numerical stability requirements and requiring little effort on part of the user.

In `SeismicMesh`, the domain geometry is defined as the 0-level set of a signed distance function (SDF). By default the program assumes a regular geometry, which implies the domain is approximated by a rectangle or a cube depending on the dimensionality of the problem. To create irregular, potentially disconnected geometries, set operations (e.g., unions and intersections) can be performed with several SDFs. The usage of SDFs to define the domain avoids the need to have explicit geometry information defining the boundary. At the same time, SDFs enable the meshing of potentially highly irregularly-shaped domains. Geometries such as the free surface boundary, seafloor, volcanoes, and salt-bodies are characterized by pronounced changes to seismic velocities making it possible to demarcate these regions by thresholding seismic velocity. Considering this, a capability is provided in `SesimicMesh` to create signed distance functions from isocontours of seismic velocity using the Fast-Marching method [@sethian1996fast].

# Parallelism

One motivation for parallelism in applications such as time-domain FWI and RTM is that relatively high source frequencies are used (e.g., 5-7 Hz) to produce high-resolution seismic velocity images. These models require a minimum number of 5 to 10 vertices per wavelength of the source wavelet to ensure the subsequent numerical simulation is numerically accurate. However, this can make the generation of tetrahedral meshes prohibitively computationally expensive for a realistic 3D inversion domain. For example, a 3D mesh of a benchmark FWI model EAGE Salt [@doi:10.1190/1.1437283] requires 507,862 cells when resolving a 2 Hz source frequency using 5 vertices per wavelength. For the same model discretized for source wavelet with a peak frequency of 4 Hz, the tetrahedral mesh increases in number of cells by a factor of approximately 8 (approximately 4,082,978 cells).

Thus, parallelism enables the rapid generation of high-quality meshes for FWI and RTM applications on the order of minutes. \autoref{fig:speedup} shows a peak speed-up of approximately 6.60 times using 11 cores when performing 50 meshing iterations with `SeismicMesh` to generate an approximately 4 million cell mesh. The usage of 11 cores reduces the generation time of this example from 20 minutes to approximately 2 wall-clock minutes. The scalability of the mesh generation algorithm is primarily limited by the size of the subdomains and fails if the subdomain becomes too thin relative to the local maximum element size [@peterka2014high]. The machine used was 2 Intel Xeon Gold 6148 clocked at 2.4 GHz (40 cores in total, 27 MB cache, 10.4 GT/s) with 192 GB of RAM connected together with a 100 Gb/s InfiniBand network.


![Speed-up (left-axis) as compared to the sequential version of the program and wall-clock time in minutes to generate a 3D mesh (approximately 4.6 M cells) for the EAGE Salt seismic velocity model. The panel on the right hand side shows the a slice through the center of the generated mesh. \label{fig:speedup}](Performance.jpg)


# Future applications

Here are some future applications for this software:

* `SeismicMesh` is being used by a group of researchers to build 2D/3D meshes for a seismological FEM model that has been developed in the Firedrake computing environment [@10.1145/2998441].

* Much like how the original `DistMesh` program has been used, `SeismicMesh` can be also applied to many domains besides seismology such as fluid dynamics, astrophysics, and coastal ocean modeling. An open source project project is already under way to use the same mesh generation technology for a Python version of `OceanMesh2D` to build industrial-grade meshes of coastal oceans [@roberts2019oceanmesh2d].

* The usage of SDF to define the meshing domain present potential use cases in a topology-optimization framework [@laurain2018level] for modeling the sharp interface of salt-bodies in seismological domains. In these applications, the 0-level set of a SDF is used to demarcate the boundary of the feature. Each inversion iteration, updates to an objective functional produce modifications to the 0-level set. In this framework, `SeismicMesh` can be embedded within the inversion algorithm to generate and adapt meshes so that they conform accurately to the 0-level set.


# Acknowledgements

This research was carried out in association with the ongoing R&D project registered as ANP 20714-2, "Software technologies for modelling and inversion, with applications in seismic imaging"  (University of São Paulo / Shell Brasil / ANP).

# References
