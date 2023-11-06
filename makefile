all: serverM serverS serverL serverH client

serverM: serverM.cpp
	g++ -o serverM serverM.cpp

serverS: serverS.cpp
	g++ -o serverS serverS.cpp

serverL: serverL.cpp
	g++ -o serverL serverL.cpp

serverH: serverH.cpp
	g++ -o serverH serverH.cpp

client: client.cpp
	g++ -o client client.cpp

clean:
	rm -rf all: serverM serverS serverL serverH client