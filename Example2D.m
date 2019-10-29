clearvars; close all;% clc;
% Read in the Marmousi ii p-wave velocity model.
% Downloaded from here: http://www.agl.uh.edu/downloads/downloads.htm
%

%-----------------------------------------------------------
%   Keith Roberts   : 2019 --
%   Email           : krober@usp.br
%   Last updated    : 10/28/2019
%-----------------------------------------------------------
%

% ensure path is set correctly
libpath
%% specify mesh design 
FNAME = 'MODEL_P-WAVE_VELOCITY_1.25m.segy'; % SegY file containing velocity model
GRIDSPACE = 1.25 ; % grid spacing (meters) p-wave model. 
OFNAME = 'OUTPUT.msh'; % output fi
MIN_EL = 25 ; % minimum element size (meters)
MAX_EL = 5e3 ;% maximum element size (meters)
WL     = 500 ;% number of nodes per wavelength of wave with FREQ (hz)
FREQ   = 15 ; % maximum shot record frequency (hz)
SLP    = 75 ; % element size (meters) near maximum gradient in P-wavepseed. 
GRADE  = 0.15 ; % expansion rate of mesh resolution (in decimal percent).
CR     = 0.1 ; % desired Courant number desired DT will be enforced for.
DT     = 1e-3; % desired time step (seconds).
%% build sizing function
gdat = geodata('segy',FNAME,'gridspace',GRIDSPACE) ;

% plot(gdat) % visualize p-wave velocity model

ef = edgefx('geodata',gdat,...
    'slp',SLP,...
    'wl',WL,'f',FREQ,...
    'dt',DT,'cr',CR,...
    'min_el',MIN_EL,'max_el',MAX_EL,...
    'g',GRADE);

% plot(fh); % visualize mesh size function

%% mesh generation step
drectangle = @(p,x1,x2,y1,y2) -min(min(min(-y1+p(:,2),y2-p(:,2)),-x1+p(:,1)),x2-p(:,1));

fd = @(p) max( drectangle(p,...
     gdat.bbox(1,1),gdat.bbox(1,2),gdat.bbox(2,1),gdat.bbox(2,2)),...
     -(sqrt(sum(p.^2,2))-0.5) );
 
fh = @(p) ef.F(p); 

P_FIX=[]; 
E_FIX=[]; 
IT_MAX=100; % DEFAULT NUMBER OF MESH GENERATION ITERATIONS 1000
FID=1;
FIT=[];

[ P, T, STAT ] = distmesh( fd, fh, MIN_EL, gdat.bbox', P_FIX, E_FIX, IT_MAX, FID, FIT ) ;


P(:,2)=P(:,2)*-1;

figure; patch( 'vertices', P, 'faces', T, 'facecolor', [.90 .90 .90] )

axis equal

%% write the mesh to disk. 
% write mesh to a msh format
gmsh_mesh2d_write ( 'output.msh', 2, length(P), P', ...
  3, length(T), T' ) ;

% write mesh to a vtu format
P(:,3) = zeros*P(:,2); % add dummy 3rd dimension
exportTriangulation2VTK('output',P,T) ;
