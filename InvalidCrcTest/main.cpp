#include <iostream>

using namespace std;

void main(int argc, char *argv[])
{
	if (argc<4) {
		cout<<argv[0]<<" <input.mp2> <output.mp2> <frameSize>"<<endl;
		return;
	}

	cout<<"Input file: "<<argv[1]<<endl;
	cout<<"Output file: "<<argv[2]<<endl;

	//Decode
	FILE *inpStream,*outStream;
	errno_t err;
	if( (err=fopen_s( &inpStream,argv[1],"rb")) !=0 ) { cout<<argv[1]<<" open failure"<<endl; return; }
	if( (err=fopen_s( &outStream,argv[2],"wb")) !=0 ) { cout<<argv[2]<<" open failure"<<endl; return; }

	unsigned char buffer[4096];
	size_t nRead;
	while(!feof(inpStream)){
		nRead = fread(buffer,1,atoi(argv[3]),inpStream);
		buffer[4]=buffer[5]=0;
		fwrite(buffer,1,nRead,outStream);
		cout<<".";
	}

	fclose(outStream);
	fclose(inpStream);
	cout<<"end of main"<<endl;
};