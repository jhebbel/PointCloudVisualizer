#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <chrono>
#include <algorithm>

#include <pcl/common/common_headers.h>
#include <pcl/visualization/pcl_visualizer.h>

#include "k4a.c"

#define clamp(x, low, high) std::max(low, std::min(high, x))

typedef struct
{
    float x;
    float y;
}
v2f;

typedef struct
{
    float x;
    float y;
    float z;
}
v3f;

typedef struct
{
    struct
    {
        float X;
        float Y;
        float Z;
    }
    Position;

    struct
    {
        float R;
        float G;
        float B;
    }
    Color;
}
point;

v3f HSV2RGB(v3f HSV) 
{
	v3f RGB;
	
    int I;
    float F, P, Q, T;
	
	float H = HSV.x;
	float S = HSV.y;
	float V = HSV.z;
	
    if (S == 0) // No saturation => grayscale; V == lightness/darkness
	{
        RGB = { V, V, V };
    }
	else
	{
		H *= 6;
		I = (int)H;
		F = H - I;
		P = V * (1 - S);
		Q = V * (1 - S * F);
		T = V * (1 - S * (1 - F));
		switch (I) {
			case 0:  RGB = {V, T, P}; break;
			case 1:  RGB = {Q, V, P}; break;
			case 2:  RGB = {P, V, T}; break;
			case 3:  RGB = {P, Q, V}; break;
			case 4:  RGB = {T, P, V}; break;
			default: RGB = {V, P, Q}; break;
		}
	}
	
	return(RGB);
}

static void PrintFPS(float DeltaTime)
{
	static int Count = 0;
	static float FrameTimeAcc = 0;
	static int StartingUp = 1;
    
    if(StartingUp)
    {
        Count++;
        if(Count > 100)
        {
            StartingUp = 0;
            Count = 0;
        }
    }
    else
    {
        int FramesToSumUp = 1000;
        if(Count == FramesToSumUp)
        {
            printf("%f ms, %f fps\n", (FrameTimeAcc/(float)FramesToSumUp), 1.0f / (FrameTimeAcc / (float)FramesToSumUp));
            FrameTimeAcc = 0;
            Count = 0;
        }
        else
        {
            FrameTimeAcc += DeltaTime;
            Count++;
        }
    }
}

int main(void)
{
    camera_config config = K4A_DEVICE_CONFIG_INIT_DISABLE_ALL;
    config.depth_mode = K4A_DEPTH_MODE_NFOV_UNBINNED;
    config.camera_fps = K4A_FRAMES_PER_SECOND_30;
    config.synchronized_images_only = false;

    tof_camera camera_ = camera_init(&config);
    tof_camera *Camera = &camera_;

    if(Camera->device)
    {
        uint32_t DepthMapWidth = Camera->max_capture_width;
        int DepthMapHeight = Camera->max_capture_height;
        int DepthMapCount = DepthMapWidth * DepthMapHeight;
        
        k4a_calibration_t calibration;
        k4a_device_get_calibration(Camera->device, config.depth_mode, config.color_resolution, &calibration);

        k4a_image_t xy_image = NULL;
        k4a_image_create(K4A_IMAGE_FORMAT_CUSTOM,
                         calibration.depth_camera_calibration.resolution_width,
                         calibration.depth_camera_calibration.resolution_height,
                         calibration.depth_camera_calibration.resolution_width * (int)sizeof(k4a_float2_t),
                         &xy_image);
        
        k4a_create_xy_table(&calibration, xy_image);
        v2f *XYMap = (v2f *)k4a_image_get_buffer(xy_image);

        size_t DepthMapSize = DepthMapCount * sizeof(uint16_t);
        uint16_t *DepthMap = (uint16_t *)malloc(DepthMapSize);

        boost::shared_ptr<pcl::visualization::PCLVisualizer> viewer (new pcl::visualization::PCLVisualizer ("3D Viewer"));
        pcl::PointCloud<pcl::PointXYZRGB>::Ptr cloud_ptr (new pcl::PointCloud<pcl::PointXYZRGB>);
        viewer->addPointCloud<pcl::PointXYZRGB>(cloud_ptr, "sample cloud");
        viewer->setBackgroundColor(0, 0, 0);
        viewer->setPointCloudRenderingProperties(pcl::visualization::PCL_VISUALIZER_POINT_SIZE, 1, "sample cloud");
        viewer->addCoordinateSystem(1.0);
        viewer->initCameraParameters();

        float DeltaTime = 0.0f;

        while(!viewer->wasStopped())
        {
            std::chrono::steady_clock::time_point Begin = std::chrono::steady_clock::now();

            camera_get_depth_map(Camera, 0, DepthMap, DepthMapSize);

            cloud_ptr->points.clear();
            
            uint32_t InsertIndex = 0;
            for(size_t i = 0; i < DepthMapCount; ++i)
            {
                float d = (float)DepthMap[i];
                
                pcl::PointXYZRGB Point;
                Point.x = -XYMap[i].x * d / 1000.0f;
                Point.y = -XYMap[i].y * d / 1000.0f;
                Point.z = d / 1000.0f;
                
                if(Point.z != 0.0f)
                {
                    // interpolate
                    float min_z = 0.5f;
                    float max_z = 3.86f;
                    
                    float hue = (Point.z - min_z) / (max_z - min_z);
                    hue = clamp(hue, 0.0f, 1.0f);

                    // the hue of the hsv color goes from red to red so we want to scale with 2/3 which is blue
                    float range = 2.0f / 3.0f;
                    
                    hue *= range;
                    hue = range - hue;

                    v3f RGB = HSV2RGB({ hue, 1.0f, 1.0f });

                    Point.r = RGB.x * 255;
                    Point.g = RGB.y * 255;
                    Point.b = RGB.z * 255;

                    cloud_ptr->points.push_back(Point);
                }
            }

            cloud_ptr->width = (int)cloud_ptr->points.size();
            cloud_ptr->height = 1;

            // display using pcl
            viewer->updatePointCloud(cloud_ptr, "sample cloud");
            viewer->spinOnce(0);

            std::chrono::steady_clock::time_point End = std::chrono::steady_clock::now();
            DeltaTime = std::chrono::duration_cast<std::chrono::microseconds>(End - Begin).count() / 1000000.0;
            PrintFPS(DeltaTime);
        }
    }

    return(0);
}