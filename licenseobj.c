/* File: licenseobj.c
 * Author: Patrick Carra
 * Class: CS-4760
 * Project 3 
 */

#include <time.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include "licenseobj.h"
#include "config.h"
#include <errno.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/types.h>
#include <stdlib.h>

struct License *license;
extern int errno;

int getlicense(){
	//Blocks until a license is availablea
	if(license->nlicenses>0){
		(license->nlicenses)--;
	}
	return 0;
}

int returnlicense(){
	//Increments the number of available licenses
	(license->nlicenses)++;
	return 0;
}

int initlicense(int number){
	//Performs any needed initialization of the license object
	license->nlicenses = number;
	return 0;
}

int addtolicenses(int n){
	//Adds n licenses to the number available
	license->nlicenses+=n;
	return 0;
}

int removelicenses(int n){
	//Decrements the number of licenses by n
	license->nlicenses-=n;
	return 0;
}
