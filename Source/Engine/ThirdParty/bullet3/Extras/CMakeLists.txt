#SUBDIRS( InverseDynamics BulletRoboticsGUI BulletRobotics obj2sdf Serialize ConvexDecomposition HACD GIMPACTUtils )
SUBDIRS( InverseDynamics Serialize ConvexDecomposition HACD GIMPACTUtils )



#Maya Dynamica plugin is moved to http://dynamica.googlecode.com

#IF (USE_GLUT AND GLUT_FOUND)
#	SUBDIRS (glui)
#ENDIF ()
