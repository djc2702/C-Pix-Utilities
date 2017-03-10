#include "pixutils.h"
#include "bmp/bmp.h"

//private methods -> make static
static pixMap* pixMap_init();
static pixMap* pixMap_copy(pixMap *p);

//plugin methods <-new for Assignment 4;
static void rotate(pixMap *p, pixMap *oldPixMap,int i, int j,void *data);
static void convolution(pixMap *p, pixMap *oldPixMap,int i, int j,void *data);
static void flipVertical(pixMap *p, pixMap *oldPixMap,int i, int j,void *data);
static void flipHorizontal(pixMap *p, pixMap *oldPixMap,int i, int j,void *data);

static pixMap* pixMap_init(){
	pixMap *p=malloc(sizeof(pixMap));
	p->pixArray_overlay=0;
	return p;
}

void pixMap_destroy (pixMap **p){
	if(!p || !*p) return;
	pixMap *this_p=*p;
	if(this_p->pixArray_overlay)
	free(this_p->pixArray_overlay);
	if(this_p->image)free(this_p->image);
	free(this_p);
	this_p=0;
}

pixMap *pixMap_read(char *filename){
	pixMap *p=pixMap_init();
	int error;
	if((error=lodepng_decode32_file(&(p->image), &(p->imageWidth), &(p->imageHeight),filename))){
		fprintf(stderr,"error %u: %s\n", error, lodepng_error_text(error));
		return 0;
	}
	p->pixArray_overlay=malloc(p->imageHeight*sizeof(rgba*));
	p->pixArray_overlay[0]=(rgba*) p->image;
	for(int i=1;i<p->imageHeight;i++){
		p->pixArray_overlay[i]=p->pixArray_overlay[i-1]+p->imageWidth;
	}
	return p;
}
int pixMap_write(pixMap *p,char *filename){
	int error=0;
	if(lodepng_encode32_file(filename, p->image, p->imageWidth, p->imageHeight)){
		fprintf(stderr,"error %u: %s\n", error, lodepng_error_text(error));
		return 1;
	}
	return 0;
}

pixMap *pixMap_copy(pixMap *p){
	pixMap *new=pixMap_init();
	*new=*p;
	new->image=malloc(new->imageHeight*new->imageWidth*sizeof(rgba));
	memcpy(new->image,p->image,p->imageHeight*p->imageWidth*sizeof(rgba));
	new->pixArray_overlay=malloc(new->imageHeight*sizeof(void*));
	new->pixArray_overlay[0]=(rgba*) new->image;
	for(int i=1;i<new->imageHeight;i++){
		new->pixArray_overlay[i]=new->pixArray_overlay[i-1]+new->imageWidth;
	}
	return new;
}


void pixMap_apply_plugin(pixMap *p,plugin *plug){
	pixMap *copy=pixMap_copy(p);
	for(int i=0;i<p->imageHeight;i++){
		for(int j=0;j<p->imageWidth;j++){
			plug->function(p,copy,i,j,plug->data);
		}
	}
	pixMap_destroy(&copy);
}

int pixMap_write_bmp16(pixMap *p,char *filename){
	BMP16map *bmp16=BMP16map_init(p->imageHeight,p->imageWidth,0,5,6,5); //initialize the bmp type
	if(!bmp16) return 1;

  for (int i = 0; i < p->imageWidth; i++) {
		for (int j = 0; j < p-> imageHeight; j++) {
			uint16_t r16 = (p->pixArray_overlay[p->imageHeight - i - 1][j]).r;
      uint16_t g16 = (p->pixArray_overlay[p->imageHeight - i - 1][j]).g;
      uint16_t b16 = (p->pixArray_overlay[p->imageHeight - i - 1][j]).b;

			//000000000 RRRRRrrr
			//&
			//000000000 11111000
			// =
			//000000000 RRRRR000 << 8
			//RRRRRR000 00000000
			r16 = (r16 & 0xF8) << 8;

			//000000000 GGGGGGgg
			//&
			//00000000 11111100
			//=
			//00000000 GGGGGG00 << 3
			//00000GGG GGG00000
      g16 = (g16 & 0xFC) << 3;

			//00000000 BBBBBbbb
			//&
			//00000000 11111000
			//=
			//00000000 BBBBB000 >> 3
			//00000000 000BBBBB
      b16 = (b16 & 0xF8) >> 3;

			bmp16->pixArray[i][j] = r16 | g16 | b16;
		}
	}

	BMP16map_write(bmp16,filename);
	BMP16map_destroy(&bmp16);
	return 0;
}
void plugin_destroy(plugin **plug){
	//free the allocated memory and set *plug to zero (NULL)
	plugin *ptr = *plug;
	if (ptr->data) free(ptr->data);
	if (*plug) free(*plug);
	*plug = 0;
}

plugin *plugin_parse(char *argv[] ,int *iptr){
	//malloc new plugin
	plugin *new=malloc(sizeof(plugin));
	new->function=0;
	new->data=0;

	int i=*iptr;
	if(!strcmp(argv[i]+2,"rotate")){
		new->function=rotate;
		new->data=malloc(2 *sizeof(float));
		float *sc=(float*) new->data;
		int i=*iptr;
		float theta=atof(argv[i+1]);
		sc[0]=sin(degreesToRadians(-theta));
		sc[1]=cos(degreesToRadians(-theta));
		*iptr=i+2;
		return new;
	}
	if(!strcmp(argv[i]+2,"convolution")){
		new->function=convolution;
		new->data=malloc(3 * sizeof(int[3]));
		int (*kernel)[3] = (int(*)[3]) new->data;
		int i = *iptr;
		kernel[0][0]=atoi(argv[i+1]);
		kernel[0][1]=atoi(argv[i+2]);
		kernel[0][2]=atoi(argv[i+3]);
		kernel[1][0]=atoi(argv[i+4]);
		kernel[1][1]=atoi(argv[i+5]);
		kernel[1][2]=atoi(argv[i+6]);
		kernel[2][0]=atoi(argv[i+7]);
		kernel[2][1]=atoi(argv[i+8]);
		kernel[2][2]=atoi(argv[i+9]);
		*iptr=i+10;
		return new;
	}
	if(!strcmp(argv[i]+2,"flipHorizontal")){
		new->function=flipHorizontal;
		*iptr=i+1;
		return new;
	}
	if(!strcmp(argv[i]+2,"flipVertical")){
		new->function=flipVertical;
		*iptr=i+1;
		return new;
	}
	return(0);
}

//define plugin functions

static void rotate(pixMap *p, pixMap *oldPixMap,int i, int j,void *data){
	float *sc=(float*) data;
	const float ox=p->imageWidth/2.0f;
	const float oy=p->imageHeight/2.0f;
	const float s=sc[0];
	const float c=sc[1];
	const int y=i;
	const int x=j;
	float rotx = c*(x-ox) - s * (oy-y) + ox;
	float roty = -(s*(x-ox) + c * (oy-y) - oy);
	int rotj=rotx+.5;
	int roti=roty+.5;
	if(roti >=0 && roti < oldPixMap->imageHeight && rotj >=0 && rotj < oldPixMap->imageWidth){
		memcpy(p->pixArray_overlay[y]+x,oldPixMap->pixArray_overlay[roti]+rotj,sizeof(rgba));
	}
	else{
		memset(p->pixArray_overlay[y]+x,0,sizeof(rgba));
	}
}

static void convolution(pixMap *p, pixMap *oldPixMap,int i, int j,void *data){
	int (*kernel)[3] = (int(*)[3]) data;
	int redacc = 0; //acccumulators
	int greenacc = 0;
	int blueacc = 0;
	int alphaacc = 0;

	int sum = 0;
	int tmpi, tmpj;
	for (int k = 0; k < 3; k++) {
		for (int l = 0; l < 3; l++) {
			sum += kernel[k][l];
			if (i > 1 && i < oldPixMap->imageHeight - 1 && j > 1 && j < oldPixMap->imageWidth - 1) {
				tmpi = i + (k - 1);
				tmpj = j + (l - 1);
			} else {
				if (i <= 1) tmpi =  2 + (k - 1);
				if (i >= oldPixMap->imageHeight - 1) tmpi = (oldPixMap->imageHeight - 2) + (k - 1);
				if (j <= 1) tmpj = 2 + (l - 1);
				if (j >= oldPixMap->imageWidth - 1) tmpj = (oldPixMap->imageWidth - 2) + (l - 1);
			}

			rgba tmppix = (oldPixMap->pixArray_overlay[tmpi][tmpj]);
			redacc += (kernel[k][l] * (unsigned int) (tmppix.r));
			greenacc += (kernel[k][l] * (unsigned int) (tmppix.g));
			blueacc += (kernel[k][l] * (unsigned int) (tmppix.b));
			alphaacc += (kernel[k][l] * (unsigned int) (tmppix.a));
		} // end inner kernel for
	} // end outer kernel for

	// normalize if the kernel elements add to more than 0
	if (sum != 0) {
		redacc /= sum;
		greenacc /= sum;
		blueacc  /= sum;
		alphaacc /= sum;
	}

	if (redacc <= 0) redacc = 0;
	if (greenacc <= 0) greenacc = 0;
	if (blueacc <= 0) blueacc = 0;
	if (alphaacc <= 0) alphaacc += 255;
	if (redacc > 255) redacc = 255;
	if (greenacc > 255) greenacc = 255;
	if (blueacc > 255) blueacc = 255;

	p->pixArray_overlay[i][j].r = redacc;
	p->pixArray_overlay[i][j].g = greenacc;
	p->pixArray_overlay[i][j].b = blueacc;
	p->pixArray_overlay[i][j].a = alphaacc;
}

//very simple functions - does not use the data pointer - good place to start

static void flipVertical(pixMap *p, pixMap *oldPixMap,int i, int j,void *data){
	//reverse the pixels vertically - can be done in one line
	memcpy(p->pixArray_overlay[i], oldPixMap->pixArray_overlay[oldPixMap->imageHeight - i - 1], oldPixMap->imageWidth * sizeof(rgba));
}

static void flipHorizontal(pixMap *p, pixMap *oldPixMap,int i, int j,void *data){
	//reverse the pixels horizontally - can be done in one line
	memcpy(&(p->pixArray_overlay[i][j]), &(oldPixMap->pixArray_overlay[i][oldPixMap->imageWidth - j - 1]), sizeof(rgba));

}
