CXX      = g++
CXXFLAGS = -g -Wall

adfscp: adfscp.o AcornADFS.o AcornFS.o DiskImgIOlinear.o DiskImgIO.o
	$(CXX) -o adfscp adfscp.o AcornADFS.o AcornFS.o DiskImgIOlinear.o DiskImgIO.o
