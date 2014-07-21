/*  
 *  These subroutines are meant to be called from a pyJHTDB.libTDB object.
 *  You're on your own otherwise.
 *
 * */

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <hdf5.h>

#include "turblib.h"

// relative error of single precision numbers
float float_error = 1e-6;

int getBline(
        char *authToken,
        char *dataset,
        float time,
        int ssteps,
        float ds,
        enum SpatialInterpolation spatial,
        enum TemporalInterpolation temporal,
        int count,
        float x[][3])
{
    int p, s;
    float bsize;
	float (*y)[3] = malloc(count*sizeof(float[3]));
	float (*bfield0)[3] = malloc(count*sizeof(float[3]));
	float (*bfield1)[3] = malloc(count*sizeof(float[3]));

    for (s = 1; s <= ssteps; s++)
    {
        // get bhat0 and compute y
        getMagneticField(authToken, dataset, time, spatial, temporal, count, x, bfield0);
        for (p = 0; p < count; p++)
        {
            bsize = sqrt(bfield0[p][0]*bfield0[p][0] + bfield0[p][1]*bfield0[p][1] + bfield0[p][2]*bfield0[p][2]);
            bfield0[p][0] /= bsize;
            bfield0[p][1] /= bsize;
            bfield0[p][2] /= bsize;
            y[p][0] = x[p][0] + ds*bfield0[p][0];
            y[p][1] = x[p][1] + ds*bfield0[p][1];
            y[p][2] = x[p][2] + ds*bfield0[p][2];
        }
        // get bhat1 and compute next position
        getMagneticField(authToken, dataset, time, spatial, temporal, count, y, bfield1);
        for (p = 0; p < count; p++)
        {
            bsize = sqrt(bfield1[p][0]*bfield1[p][0] + bfield1[p][1]*bfield1[p][1] + bfield1[p][2]*bfield1[p][2]);
            bfield1[p][0] /= bsize;
            bfield1[p][1] /= bsize;
            bfield1[p][2] /= bsize;
            x[p+count][0] = x[p][0] + .5*ds*(bfield0[p][0] + bfield1[p][0]);
            x[p+count][1] = x[p][1] + .5*ds*(bfield0[p][1] + bfield1[p][1]);
            x[p+count][2] = x[p][2] + .5*ds*(bfield0[p][2] + bfield1[p][2]);
        }
        x += count;
    }
    free(bfield0);
    free(bfield1);
    free(y);
    return 0;
}

int getRectangularBoundedBline(
        char *authToken,
        char *dataset,
        float time,
        int maxsteps,
        float ds,
        enum SpatialInterpolation spatial,
        enum TemporalInterpolation temporal,
        int count,
        float traj[][3],
        int traj_length[],
        float xmin, float xmax,
        float ymin, float ymax,
        float zmin, float zmax)
{
    // traj will contain all the trajectories, and traj_length will contain the lengths of all trajectories
    // the shape of traj is assumed to be (number of particles, allocated_traj_length, 3)
    int p, s;
    float bsize;
    float (*x)[3];
	float y[1][3];
	float bfield0[1][3];
	float bfield1[1][3];
   
    // keep a little way away from the actual edge...
    xmin *= 1 + (xmin > 0 ? -1 : +1)*float_error;
    ymin *= 1 + (ymin > 0 ? -1 : +1)*float_error;
    zmin *= 1 + (zmin > 0 ? -1 : +1)*float_error;
    xmax *= 1 + (xmax > 0 ? +1 : -1)*float_error;
    ymax *= 1 + (ymax > 0 ? +1 : -1)*float_error;
    zmax *= 1 + (zmax > 0 ? +1 : -1)*float_error;

    //loop after particles
    for (p = 0; p < count; p++)
    {
        // x is at the start of the current trajectory
        x = traj + p*(maxsteps+1);
        for (s = 1; s <= maxsteps; s++)
        {
            // get bhat0 and compute y
            getMagneticField(authToken, dataset, time, spatial, temporal, 1, x, bfield0);
            bsize = sqrt(bfield0[0][0]*bfield0[0][0]
                       + bfield0[0][1]*bfield0[0][1]
                       + bfield0[0][2]*bfield0[0][2]);
            bfield0[0][0] /= bsize;
            bfield0[0][1] /= bsize;
            bfield0[0][2] /= bsize;
            y[0][0] = x[0][0] + ds*bfield0[0][0];
            y[0][1] = x[0][1] + ds*bfield0[0][1];
            y[0][2] = x[0][2] + ds*bfield0[0][2];
            // get bhat1 and compute next position
            getMagneticField(authToken, dataset, time, spatial, temporal, 1, y, bfield1);
            bsize = sqrt(bfield1[0][0]*bfield1[0][0]
                       + bfield1[0][1]*bfield1[0][1]
                       + bfield1[0][2]*bfield1[0][2]);
            bfield1[0][0] /= bsize;
            bfield1[0][1] /= bsize;
            bfield1[0][2] /= bsize;
            x[1][0] = x[0][0] + .5*ds*(bfield0[0][0] + bfield1[0][0]);
            x[1][1] = x[0][1] + .5*ds*(bfield0[0][1] + bfield1[0][1]);
            x[1][2] = x[0][2] + .5*ds*(bfield0[0][2] + bfield1[0][2]);
            x += 1;
            if (((*x)[0] < xmin || (*x)[0] > xmax)
             || ((*x)[1] < ymin || (*x)[1] > ymax)
             || ((*x)[2] < zmin || (*x)[2] > zmax))
                break;
        }
        traj_length[p] = s;
    }
    return 0;
}

int getSphericalBoundedBline(
        char *authToken,
        char *dataset,
        float time,
        int maxsteps,
        float ds,
        enum SpatialInterpolation spatial,
        enum TemporalInterpolation temporal,
        int count,
        float traj[][3],
        int traj_length[],
        float ox,
        float oy,
        float oz,
        float radius)
{
    // traj will contain all the trajectories, and traj_length will contain the lengths of all trajectories
    // the shape of traj is assumed to be (number of particles, allocated_traj_length, 3)
    int p, s;
    float bsize, rad0, rad1, stmp, xtmp, ytmp, ztmp, valtmp;
    float (*x)[3];
	float y[1][3];
	float bfield0[1][3];
	float bfield1[1][3];

    radius *= 1 + float_error;

    //loop after particles
    for (p = 0; p < count; p++)
    {
        // x is at the start of the current trajectory
        x = traj + p*(maxsteps+1);
        for (s = 1; s <= maxsteps; s++)
        {
            // get bhat0 and compute y
            getMagneticField(authToken, dataset, time, spatial, temporal, 1, x, bfield0);
            bsize = sqrt(bfield0[0][0]*bfield0[0][0]
                       + bfield0[0][1]*bfield0[0][1]
                       + bfield0[0][2]*bfield0[0][2]);
            bfield0[0][0] /= bsize;
            bfield0[0][1] /= bsize;
            bfield0[0][2] /= bsize;
            y[0][0] = x[0][0] + ds*bfield0[0][0];
            y[0][1] = x[0][1] + ds*bfield0[0][1];
            y[0][2] = x[0][2] + ds*bfield0[0][2];
            // get bhat1 and compute next position
            getMagneticField(authToken, dataset, time, spatial, temporal, 1, y, bfield1);
            bsize = sqrt(bfield1[0][0]*bfield1[0][0]
                       + bfield1[0][1]*bfield1[0][1]
                       + bfield1[0][2]*bfield1[0][2]);
            bfield1[0][0] /= bsize;
            bfield1[0][1] /= bsize;
            bfield1[0][2] /= bsize;
            x[1][0] = x[0][0] + .5*ds*(bfield0[0][0] + bfield1[0][0]);
            x[1][1] = x[0][1] + .5*ds*(bfield0[0][1] + bfield1[0][1]);
            x[1][2] = x[0][2] + .5*ds*(bfield0[0][2] + bfield1[0][2]);
            if ((rad1 = sqrt((x[1][0] - ox)*(x[1][0] - ox)
                           + (x[1][1] - oy)*(x[1][1] - oy)
                           + (x[1][2] - oz)*(x[1][2] - oz))) > radius)
            {
                //rad0 = sqrt((x[0][0] - ox)*(x[0][0] - ox)
                //          + (x[0][1] - oy)*(x[0][1] - oy)
                //          + (x[0][2] - oz)*(x[0][2] - oz));
                //valtmp = (x[0][0] - ox)*(x[1][0] - ox)
                //       + (x[0][1] - oy)*(x[1][1] - oy)
                //       + (x[0][2] - oz)*(x[1][2] - oz);
                //stmp = (2*(1 - valtmp) + sqrt(4*(1 - 2*valtmp + valtmp*valtmp) + 4*radius*radius*(rad1*rad1 + rad0*rad0 - 2*valtmp))) / (2 * (rad1*rad1 + rad0*rad0 - 2*valtmp));
                //xtmp = x[0][0]*(1 - stmp) + stmp * x[1][0];
                //ytmp = x[0][1]*(1 - stmp) + stmp * x[1][1];
                //ztmp = x[0][2]*(1 - stmp) + stmp * x[1][2];
                //fprintf(stderr, "%g %g %g\n", stmp, xtmp, x[1][0]);
                //stmp = (2*(1 - valtmp) - sqrt(4*(1 - 2*valtmp + valtmp*valtmp) + 4*radius*radius*(rad1*rad1 + rad0*rad0 - 2*valtmp))) / (2 * (rad1*rad1 + rad0*rad0 - 2*valtmp));
                //fprintf(stderr, "%g %g %g\n", stmp, xtmp, x[1][0]);
                //x[1][0] = xtmp;
                //x[1][1] = ytmp;
                //x[1][2] = ztmp;
                break;
            }
            x += 1;
        }
        traj_length[p] = s;
    }
    return 0;
}

int getMagneticFieldDebug(
        char *authToken,
        char *dataset,
        float time,
        enum SpatialInterpolation spatial,
        enum TemporalInterpolation temporal,
        int count,
        float datain[][3],
        float dataout[][3])
{
    int p;
    for (p = 0; p < count; p++)
    {
        dataout[p][0] = 1;
        dataout[p][1] = 0;
        dataout[p][2] = 0;
    }
    return 0;
}

int getSphericalBoundedBlineDebug(
        char *authToken,
        char *dataset,
        float time,
        int maxsteps,
        float ds,
        enum SpatialInterpolation spatial,
        enum TemporalInterpolation temporal,
        int count,
        float traj[][3],
        int traj_length[],
        float ox,
        float oy,
        float oz,
        float radius)
{
    // traj will contain all the trajectories, and traj_length will contain the lengths of all trajectories
    // the shape of traj is assumed to be (number of particles, allocated_traj_length, 3)
    int p, s;
    float bsize, rad0, rad1, stmp, xtmp, ytmp, ztmp, valtmp;
    float (*x)[3];
	float bfield0[1][3];

    ds = (ds > 0? +1 : -1) *radius / 100;

    radius *= 1 + float_error;

    //loop after particles
    for (p = 0; p < count; p++)
    {
        // x is at the start of the current trajectory
        x = traj + p*(maxsteps+1);
        for (s = 1; s <= maxsteps; s++)
        {
            rad0 = sqrt((x[0][0] - ox)*(x[0][0] - ox)
                      + (x[0][1] - oy)*(x[0][1] - oy)
                      + (x[0][2] - oz)*(x[0][2] - oz));
            getMagneticFieldDebug(authToken, dataset, time, spatial, temporal, 1, x, bfield0);
            x[1][0] = x[0][0] + ds*bfield0[0][0];
            x[1][1] = x[0][1] + ds*bfield0[0][1];
            x[1][2] = x[0][2] + ds*bfield0[0][2];
            rad1 = sqrt((x[1][0] - ox)*(x[1][0] - ox)
                      + (x[1][1] - oy)*(x[1][1] - oy)
                      + (x[1][2] - oz)*(x[1][2] - oz));
            if (rad1 > radius)
            {
                //rad0 = sqrt((x[0][0] - ox)*(x[0][0] - ox)
                //          + (x[0][1] - oy)*(x[0][1] - oy)
                //          + (x[0][2] - oz)*(x[0][2] - oz));
                //valtmp = (x[0][0] - ox)*(x[1][0] - ox)
                //       + (x[0][1] - oy)*(x[1][1] - oy)
                //       + (x[0][2] - oz)*(x[1][2] - oz);
                //stmp = (2*(1 - valtmp) + sqrt(4*(1 - 2*valtmp + valtmp*valtmp) + 4*radius*radius*(rad1*rad1 + rad0*rad0 - 2*valtmp))) / (2 * (rad1*rad1 + rad0*rad0 - 2*valtmp));
                //xtmp = x[0][0]*(1 - stmp) + stmp * x[1][0];
                //ytmp = x[0][1]*(1 - stmp) + stmp * x[1][1];
                //ztmp = x[0][2]*(1 - stmp) + stmp * x[1][2];
                //fprintf(stderr, "%g %g %g\n", stmp, xtmp, x[1][0]);
                //stmp = (2*(1 - valtmp) - sqrt(4*(1 - 2*valtmp + valtmp*valtmp) + 4*radius*radius*(rad1*rad1 + rad0*rad0 - 2*valtmp))) / (2 * (rad1*rad1 + rad0*rad0 - 2*valtmp));
                //fprintf(stderr, "%g %g %g\n", stmp, xtmp, x[1][0]);
                //x[1][0] = xtmp;
                //x[1][1] = ytmp;
                //x[1][2] = ztmp;
                break;
            }
            x += 1;
        }
        traj_length[p] = s;
    }
    return 0;
}

int set_custom_dataset_description(float dx, float dt, int size)
{
    extern set_info DataSets[8];
    DataSets[7].dx = dx;
    DataSets[7].dt = dt;
    DataSets[7].size = size;
    return 0;
}

int read_from_file(
        char *dataset,
        int tindex,
        int xindex, int xsize,
        int yindex, int ysize,
        int zindex, int zsize,
        float *ufield,
        float *bfield)
{
    TurbDataset dataset_ = getDataSet(dataset);
    hsize_t dim_mem[] = {zsize, ysize, xsize, 3};
    hid_t mspace = H5Screate_simple(4, dim_mem, NULL);
    loadSubBlock(
            dataset_,
            turb_velocity,
            tindex,
            mspace,
            ufield,
            xindex, yindex, zindex,
            xsize,
            ysize,
            zsize,
            0, 0, 0);
    loadSubBlock(
            dataset_,
            turb_magnetic,
            tindex,
            mspace,
            bfield,
            xindex, yindex, zindex,
            xsize,
            ysize,
            zsize,
            0, 0, 0);
    return 0;
}

