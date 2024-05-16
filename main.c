#include <stdio.h>
#include<stdlib.h>
#include<sys/stat.h>
#include<dirent.h>
#include<stdint.h>
#include<string.h>
#include<time.h>
#include <errno.h>
#include <unistd.h>
#include <limits.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>

#define MAX_PATH 50
#define MAX_DATA 1000
#define MAX_OUTPUT_PATH 16384

typedef struct 
{
	int inodeNum;
	char name[250];
	int size;
	time_t lastAccess; /* time of last access */
	time_t lastModifiaction; /* time of last modification */
	mode_t permissions;
}Metadata;

typedef struct{
	char dirName[256];
	Metadata dirData;
	Metadata snapFilesData[MAX_DATA];
	int nrEntries;
}Snapshot;

Metadata getMetadata(char path[])
{
	struct stat st;
	Metadata metadata;

	if(stat(path,&st) == -1)
	{
		printf("error\n");
		exit(-1);
	}

	strcpy(metadata.name,path);
	metadata.size=st.st_size;
	metadata.inodeNum=st.st_ino;
	metadata.lastAccess = st.st_atime;
    metadata.lastModifiaction = st.st_mtime;
	metadata.permissions = st.st_mode;

	return metadata;
}

Snapshot createSnapshot(char nameDirectory[],int nivel)
{
	DIR *dir = opendir(nameDirectory);
	struct stat dst;
	char* nume;
	int n;

	Snapshot snapshot={0}; //The {0} initializes all members of the Snapshot structure to zero or null values.

	if(dir==NULL)
	{
		printf("The director could not be open.\n");
		exit(-1);
	}

	if((lstat(nameDirectory,&dst))<0)
	{
		perror("eroare la lstat");
		exit(-1);
	}

	strcpy(snapshot.dirName,nameDirectory);
	snapshot.dirData.inodeNum=dst.st_ino;
	snapshot.dirData.size=dst.st_size;
	snapshot.dirData.lastAccess=dst.st_atime;
	snapshot.dirData.lastModifiaction=dst.st_mtime;
	snapshot.dirData.permissions=dst.st_mode;

	snapshot.nrEntries=0;
	
	struct dirent *entry;
	struct stat st;
	char path[MAX_PATH];
	char pathLink[MAX_PATH+1];

	while ((entry=readdir(dir))!=NULL)
	{
		nume=entry->d_name;

		if(strcmp(nume, ".") == 0 || strcmp(nume, "..")==0 || strcmp(nume,".Snapshot.txt")==0)
	        continue;

		//creez calea relativa/absoluta
		snprintf(path,sizeof(path),"%s/%s",nameDirectory,nume); //am afisat in path numele directorului tata si numele continutului sub un anumit format

		if((lstat(path,&st))<0)	//lstat returneaza 0 daca apelul a decurs cu succes si -1 in caz contrar	
		{
			perror("eroare la lstat");
			exit(-1);
		}

		Metadata metadata=getMetadata(path);

		//adaug in structura mea metadata
		if(snapshot.nrEntries<MAX_DATA)
		{
			snapshot.snapFilesData[snapshot.nrEntries]=metadata;
		}

		if(S_ISDIR(st.st_mode) && nivel==0)
		{
			//createSnapshot(path,nivel+1);
		}
		else if(S_ISLNK(st.st_mode))
		{
			n = readlink(path, pathLink, sizeof(pathLink));
          	pathLink[n]='\0';
		}
		snapshot.nrEntries++;
	}

	//creare .Snapshot.txt file
	snprintf(path,sizeof(path),"%s%s",nameDirectory,"/.Snapshot.txt");
	int fd=open(path,O_WRONLY | O_CREAT | O_TRUNC,S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

	if(fd==-1)
	{
		perror("Error opening\n");
		exit(-1);
	}

	if(write(fd,&snapshot,sizeof(Snapshot))!=sizeof(Snapshot))
	{
		perror("Error writing to file");
        exit(EXIT_FAILURE);
	}
	else{
		close(fd);
	}

	closedir(dir);
	return snapshot;
}

Snapshot readSnapshot(char nameDir[])
{
	char path[MAX_PATH];
	struct dirent * entry;
	DIR *dir = opendir(nameDir);
	int ok=0;

	snprintf(path,sizeof(path),"%s/.Snapshot.txt",nameDir);

	while((entry=readdir(dir))!=NULL)
	{
		if(strcmp(entry->d_name,".Snapshot.txt")==0)
		{
			ok=1;
			break;
		}
	}
	//daca nu il gaseste il va crea 
	if(ok==0)
	{
		createSnapshot(nameDir,0);
	}
	
	//deschid fisierul de snapshot
	int fd=open(path,O_RDONLY);

	if(fd==-1)
	{
		perror("Error opening Snapshot file!\n");
		exit(-1);
	}

	Snapshot snapshot;

	//citesc in snapshot ce scrie in fieier
	int rd=read(fd,&snapshot,sizeof(Snapshot));

	if(rd==-1)
	{
		perror("Error reading file!\n");
		exit(-1);
	}

	close(fd);
	return snapshot;
}

void printMetadata(Metadata meta,char spatii[])
{
	printf("%s[NAME] %s\n",spatii,meta.name);
	printf("%s[INODE NUMBER] %d\n",spatii,meta.inodeNum);
	printf("%s[SIZE] %d\n",spatii,meta.size);
	printf("%s[LAST ACCESS] %s",spatii,ctime(&meta.lastAccess));
	printf("%s[LAST MODIFICATION] %s",spatii,ctime(&meta.lastModifiaction));
	printf("%s[PERMISSIONS] %o\n",spatii,meta.permissions); // %o - baza 8
	printf("%s[TYPE]: %s\n",spatii, S_ISDIR(meta.permissions) ? "Directory" : "File");
	printf("%s------------------------------------\n",spatii);
}

void printSnapshot(Snapshot snap,int nivel)
{
	char spatii[MAX_PATH];

	memset(spatii,' ',5*nivel);
	spatii[5*nivel]='\0';
	
	if(nivel==0)
	{
		printf("\n%sCurrent Directory: %s\n",spatii,snap.dirName);
		printf("%sDirectory Inode: %d\n",spatii,snap.dirData.inodeNum);
		printf("%sDirectory Size: %d\n",spatii,snap.dirData.size);
		printf("%sDirectory Last Access: %s",spatii,ctime(&snap.dirData.lastAccess));
		printf("%sDirectory Last Modif: %s",spatii,ctime(&snap.dirData.lastModifiaction));
		printf("%sDirectory Permissions: %o\n",spatii,snap.dirData.permissions);
		printf("\n%sNumber Entries: %d\n\n",spatii,snap.nrEntries);
		printf("%sDirectory Entries: \n\n",spatii);

	}
	else{
		printf("%sCurrent Directory: %s\n",spatii,snap.dirName);
		printf("%sNumber Entries: %d\n\n",spatii,snap.nrEntries);
	}

	for(int i=0;i<snap.nrEntries;i++)
	{
		printMetadata(snap.snapFilesData[i],spatii);

		if(S_ISDIR(snap.snapFilesData[i].permissions) && nivel==0)
		{
			Snapshot subSnap=readSnapshot(snap.snapFilesData[i].name);

			printSnapshot(subSnap,nivel+1);
		}
		
	}
}

void printModifications(Snapshot old,Snapshot new,int nivel)
{
	char spatii[MAX_PATH];

	int found=0;
	int modificat=0;

	memset(spatii,' ',5*nivel); //l am creat pentru a indenta frumos la afisare
	spatii[5*nivel]='\0';

	if(old.dirData.inodeNum!=new.dirData.inodeNum)
	{
		perror("Error");
		return;
	}

	if(nivel==0) //daca ma aflu in directorul principal
	{
		printf("\n%s------------------------------------\n",spatii);
		printf("SNAPSHOT FOR %s DIRECTORY\n",new.dirName);
		printf("%s------------------------------------\n",spatii);
		printf("%sCURRENT DIRECTORY: %s\n",spatii,new.dirName);
		printf("%s[INODE NUMBER] %d\n",spatii,new.dirData.inodeNum);
		printf("%s[SIZE] %d\n",spatii,new.dirData.size);
		printf("%s[LAST ACCESS] %s",spatii,ctime(&new.dirData.lastAccess));
		printf("%s[LAST MODIFICATION] %s",spatii,ctime(&new.dirData.lastModifiaction));
		printf("%s[PERMISSIONS] %o\n",spatii,new.dirData.permissions);
		printf("\n%sNumber Of Entries: %d\n\n",spatii,new.nrEntries);
		printf("%sCurrent Directory Entries: \n\n",spatii);
	}
	else{ //aici ma aflu in subdirector
		printf("%sCURRENT DIRECTORY: %s\n",spatii,new.dirName);
		printf("%sNumber Of Entries: %d\n\n",spatii,new.nrEntries);
	}

	for(int i=0;i<new.nrEntries;i++)
	{
		for(int j=0;j<old.nrEntries;j++)
		{
			if(new.snapFilesData[i].inodeNum==old.snapFilesData[j].inodeNum)
			{
				found=1; // s a gasit inodeNum ul nou in cel vechi, deci exista

				if(strcmp(old.snapFilesData[j].name,new.snapFilesData[i].name)!=0)
				{
					printf("%s*[NAME] %s\n",spatii,new.snapFilesData[i].name);
					modificat=1;
				}
				else {
					printf("%s[NAME] %s\n",spatii,new.snapFilesData[i].name);
				}
				
				printf("%s[INODE NUMBER] %d\n",spatii,new.snapFilesData[i].inodeNum);

				if(old.snapFilesData[j].size!=new.snapFilesData[i].size)
				{	
					printf("%s*[SIZE] %d\n",spatii,new.snapFilesData[i].size);
					modificat=1;
				}
				else {
					printf("%s[SIZE] %d\n",spatii,new.snapFilesData[i].size);
				}

				if(old.snapFilesData[j].lastAccess!=new.snapFilesData[i].lastAccess)
				{	
					printf("%s*[LAST ACCESS] %s",spatii,ctime(&new.snapFilesData[i].lastAccess));

					modificat=1;	
				}
				else {
					printf("%s[LAST ACCESS] %s",spatii,ctime(&new.snapFilesData[i].lastAccess));
				}

				if(old.snapFilesData[j].lastAccess!=new.snapFilesData[i].lastAccess)
				{	
					printf("%s[LAST MODIFICATION] %s",spatii,ctime(&new.snapFilesData[i].lastModifiaction));

					modificat=1;
				}
				else {
					printf("%s[LAST MODIFICATION] %s",spatii,ctime(&new.snapFilesData[i].lastModifiaction));
				}

				if(old.snapFilesData[j].permissions!=new.snapFilesData[i].permissions)
				{	
					printf("%s[PERMISSIONS] %o\n",spatii,new.snapFilesData[i].permissions); // %o - baza 8

					modificat=1;
				}
				else {
					printf("%s[PERMISSIONS] %o\n",spatii,new.snapFilesData[i].permissions); // %o - baza 8
				}

				printf("%s[TYPE]: %s\n",spatii, S_ISDIR(new.snapFilesData[i].permissions) ? "Directory" : "File");

				printf("%s------------------------------------\n",spatii);
			}
		}

		if(found==0) //daca nu gasesc inode ul din cel nou in cel vechi inseamna ca s-a creat un nou fisier/director
		{
			if(S_ISDIR(new.snapFilesData[i].permissions))
			{
				printf("\n%s*** NEW DIRECTORY HAS BEEN CREATED ***\n",spatii);

				printMetadata(new.snapFilesData[i],spatii);
			}
			else
			{
				printf("\n%s*** NEW FILE HAS BEEN CREATED ***\n",spatii);

				printMetadata(new.snapFilesData[i],spatii);
			}
		}

		found=0;

		//trec recursiv si prin subdirectoare
		if(S_ISDIR(new.snapFilesData[i].permissions) && nivel==0)
		{
			Snapshot subSnapOld=readSnapshot(new.snapFilesData[i].name);
			Snapshot subSnapNew=createSnapshot(new.snapFilesData[i].name,0);
			printModifications(subSnapOld,subSnapNew,nivel+1);
		}

		
	}

	//Searching for the deleted file
	for(int i=0;i<old.nrEntries;i++)
	{
		found=0;
		for(int j=0;j<new.nrEntries;j++) // travel through the entries of the directory searching if an inode from old snapfile is in the new snapfile
		{
			if(old.snapFilesData[i].inodeNum==new.snapFilesData[j].inodeNum)
			{
				found=1;
				break;
			}
		}

		if(found==0)
		{
			printf("\n%s*** %s DELETED ***\n\n",spatii,old.snapFilesData[i].name);
		}
	}

	if(modificat==1)
	{
		printf("\n%s*** CURRENT DIRECTORY WAS MODIFIED ***\n",spatii);
		putchar('\n');
		putchar('\n');
	}
	modificat=0;
}


int main(int argc,char **argv) {

	struct stat st;

	int pozDir=0;
	int okOut=0;
	char outputDir[MAX_DATA];
	char outputPath[MAX_OUTPUT_PATH];

	char outputSafeDir[MAX_DATA];
	char outputSafePath[MAX_OUTPUT_PATH];
	int okSafe=0;
	int pozSafeDir=0;

	Snapshot old[10];
	Snapshot new;

	if(argc>11)
	{
		perror("Prea multe argumente!\n");
	}

	for(int i=1;i<argc;i++)
	{
		if(strcmp(argv[i],"-o")==0)
		{
			if(i+1<argc)
			{
				strncpy(outputDir,argv[i+1],MAX_PATH-1);
				outputDir[MAX_PATH-1]='\0';
				pozDir=i+1;
				okOut=1;
			}
			else{
				perror("Output directory not provided:(\n");
				exit(-1);
			}
			continue;
		}

		if(strcmp(argv[i],"-s")==0)
		{
			if(i+1<argc)
			{
				strncpy(outputSafeDir,argv[i+1],MAX_PATH-1);
				outputSafeDir[MAX_PATH-1]='\0';
				pozSafeDir=i+1;
				okSafe=1;
			}
			else{
				perror("Safe output directory not provided:(\n");
				exit(-1);
			}
			continue;
		}

		old[i]=readSnapshot(argv[i]);
	}

	
	int pfd[10][2]; //fiind mai multe procese in paralel am nevoie de un vector de pipe uri care sa le tina evidenta, pentru a vredea care pipe corespunde unui proces

	pid_t pid[10]={0};
	

	for(int i=1;i<argc;i++)
	{
		for(int j=i+1;j<argc;j++)
		{
			if(strcmp(argv[i],argv[j])==0)
			{
				printf("%s %s\n",argv[i],argv[j]);
				perror("They should not be equal!\n");
				exit(-1);
			}
		}

		if(i==pozDir-1 || i==pozDir || i==pozSafeDir-1 || i==pozSafeDir) continue;

		if((lstat(argv[i],&st))<0)
		{
			perror("eroare la lstat.");
			exit(-1);
		}


		//creez pipe ul
		if(pipe(pfd[i-1])==-1)
		{
			perror("Pipe creation failed.");
			exit(-1);
		}


		if(S_ISDIR(st.st_mode))
		{
			pid[i-1]=fork(); //creez procesul copil

			if(pid[i-1] == -1)
			{
				perror("Error creating child process.");
				exit(1);
			}


			if(pid[i-1] == 0) // procesul copil pentru creearea snapshot-ului
			{
				//old=readSnapshot(argv[i]);
				new=createSnapshot(argv[i],0);
				//printModifications(old,new,0);

				if(new.nrEntries>=0)
				{
					printf("Snapshot for Directory %d -> \"%s\" created successfully.\n",i,argv[i]);
				}
				else printf("Snapshot for Directory \"%s\" failed to create.\n",argv[i]);
				
				int count=0;

				//Verificarea permisiunilor si analiza sintactica prin script. Plus mutarea in safe directory daca este necesar
				for(int iter=0;iter<new.nrEntries;iter++)
				{
					if(strcmp(new.snapFilesData[iter].name,".Snapshot.txt")==0) continue;

					if(S_ISREG(new.snapFilesData[iter].permissions)==1 && (new.snapFilesData[iter].permissions & (S_IRWXU | S_IRWXG | S_IRWXO))==0)
					{
						count++;

						int pfd2[2];
						if(pipe(pfd2)==-1)
						{
							perror("Error creating the pipe for the nephew:(.");
							exit(-1);
						}

						printf("File %s in directory \"%s\" is a possible threat\n",new.snapFilesData[iter].name,argv[i]);

						//Creearea procesului pentru analiza sintactica a fisierului
						pid_t threatPID=fork();

						if(threatPID == -1)
						{
							perror("Error creating the process for the potential threat file.");
							exit(1);
						}

						if(threatPID==0) // sunt in procesul nepot
						{
							close(pfd2[0]); //inchid capatul de citire;
							dup2(pfd2[1],STDOUT_FILENO); // sau dup2(pfd2[1],1); Copiez de la stdout in pfd2[1]
							close(pfd2[1]);

							chmod(new.snapFilesData[iter].name,0777); //ii dau drepturi pentru a putea lucra cu el
							if(execlp("bash", "bash", "./verify_for_malicious.sh",new.snapFilesData[iter].name, NULL)==-1)
							{
								perror("Execlp nu functioneaza:((((\n");
								exit(-1);
							} 
						}
						else{ //procesul copil
							int threatStatus;

							wait(&threatStatus);

							chmod(new.snapFilesData[iter].name,0000);

							if(WIFEXITED(threatStatus))
							{
								printf("\nThe analysis for the potential threat file: %s completed with exit %d\n",new.snapFilesData[iter].name,WEXITSTATUS(threatStatus));

								char buff[BUFSIZ]={0};
								close(pfd2[1]);
								if(read(pfd2[0],buff,sizeof(buff))==-1)
								{
									perror("Error reading from pipe in the nephew.");
									exit(-1);
								}
								close(pfd2[0]);
								
								printf("Script returned: %s",buff);

								if(WEXITSTATUS(threatStatus)==0) //verifica valoarea returnata de script prin status
								{
									printf("This file is safe\n\n");
								}
								else{
									printf("This file is not safe. It will be moved to \"izolated_space_dir\"\n");
									if(okSafe==1) //verific daca directorul de safe se afla in linia de comanda
									{
										int size=strlen(argv[i])+1;

										sprintf(outputSafePath,"%s/%s",outputSafeDir,buff+size); //creez path-ul pentru a putea sa il mut in directorul safe
										outputSafePath[strlen(outputSafePath)-1]='\0'; //script ul are '\n' la echo iar pentru a-l sterge folosesc aceasta comanda
										if(rename(new.snapFilesData[iter].name,outputSafePath) != 0) //pur si simplu mut fisierul malitios daca buff=numele fisierului
										{
											perror("Error moving the malicious file into the directory:(\n");
											exit(-1);
										}
										else printf("Malicious file moved to safe directory!\n\n");
									}
								}
							}
							else{
								printf("Potential threat file analysis terminated abnormally\n");
							}
						}
					}
				}

				close(pfd[i-1][0]); //inchid partea de pipe in care citesc
				if(write(pfd[i-1][1],&count,sizeof(int))==-1) //scriu count-ul fisierelor malitioase in pipe pentru a-l putea comunica parintelui
				{
					perror("Error writing in pipe");
					exit(-1);
				}
				close(pfd[i-1][1]);

			
				//UPDATE crearea directorului din cod cu stat(daca returneaza -1 inseamna ca nu exista si trebuie creat)
				if(okOut==1) //daca exista director de output in linia de comanda voi copia in el snapshot-urile
				{
					snprintf(outputPath,sizeof(outputPath),"%s/%s.Snapshot.txt",outputDir,argv[i]);
					int fd=open(outputPath,O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

					if(fd==-1)
					{
						perror("Error opening file for copy\n");
						exit(-1);
					}

					int wr=write(fd,&new,sizeof(Snapshot));

					if(wr==-1)
					{
						perror("Error writing output file\n");
						exit(-1);
					}
					close(fd);
				}

				exit(0); //asa ies din procesul copil
			}
		}else
			{
				perror("The program accepts only directory!\n");
				exit(-1);
			}
	}
	
	//sunt in procesul parinte!!
	for(int i=1;i<argc;i++)
	{
		int status;
		int countCorruptedFile=0;

		if(i==pozDir-1 || i==pozDir || i==pozSafeDir-1 || i==pozSafeDir) continue;

		int rpid = wait(&status);

		int j=0;
		for(j=0;j<argc;j++) //caut pid-ul care este egal cu rpid pentru a le sincroniza cat de cat
		{
			if(rpid==pid[j])
			{
				break;
			}
		}

		close(pfd[j][1]); // inchid partea de scriere a pipe ului
		if (read(pfd[j][0], &countCorruptedFile, sizeof(int)) == -1) {
			perror("Error reading from the pipe");
			exit(-1);
		}
		close(pfd[j][0]); 

		if(WIFEXITED(status))
		{
			printf("Child process %d terminated with PID %d and exit code %d and with %d files with potential danger.\n",j+1,rpid,WEXITSTATUS(status),countCorruptedFile);
		}
		else{
			printf("Child %d ended abnormally\n",i);
		}

	}

	for(int i=1;i<argc;i++)
	{
		if(i==pozDir-1 || i==pozDir || i==pozSafeDir-1 || i==pozSafeDir) continue;

		new=readSnapshot(argv[i]);
		printModifications(old[i],new,0);
	}

	return 0;
}
