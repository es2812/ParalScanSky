# ParalScanSky

Paralelization of secuencial code used to count the number of different objects visible in an image of deep space, obtained by a telescope. 

The images have been processed, discretizing the luminosity in 16 different levels of color. Pixels that represent empty space have index 0, pixels that represent similar enough color have the same value between 1 and 15.

The image is loaded from a plain text file, the file contains one integer in each line. The two first lines contain the number of rows and columns in the image. The rest are numbers between 0 and 15 with the values of each pixel, entered by row.

Pixels with the same index that are together, be it horizontally or vertically (not diagonally), are considered the same object. The program labels each object in the image, giving each pixel of an object the same label. The program counts the number of different labels to obtain the number of different objects. Pixels with index 0 (empty space) are not labeled.
