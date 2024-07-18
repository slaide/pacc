// link with: "-lvulkan -lxcb -lxcb-util -lxcb-randr"

// to include nanosleep
#define _POSIX_C_SOURCE 199309L

#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include<unistd.h>
#include<util/util.h>
#include<stdlib.h>
#include<string.h>

#include <vulkan/vulkan_core.h>
#include<xcb/xcb.h>
#include<xcb/randr.h>
#include <xcb/xproto.h>

void check_cookie(xcb_connection_t*x_conn,xcb_void_cookie_t*cookie,const char*msg){
	xcb_generic_error_t *error=xcb_request_check(x_conn,*cookie);
	if(error){
        if(error->response_type!=0)fatal("invalid error response type %d",error->response_type);
        switch(error->error_code){
            case 0:break;
            case XCB_REQUEST:fatal("bad request code");
            case XCB_VALUE:fatal("bad value (e.g. out of range)");
            case XCB_WINDOW:fatal("bad window parameter");
            case XCB_ATOM:fatal("bad atom");
            case XCB_ACCESS:fatal("failed request due to access control violation");
            case XCB_ALLOC:fatal("failed request due to lack of resources");
            default: fatal("xcb error: %d",error->response_type);
        }
		// TODO check if error actually has error code. error is only null if the function CANNOT return an error, not when there is no error
		fatal("error during %s: code %d",msg,error->error_code);
	}
}

/*
get true length of code page 437 string, which: \
if length is shorter than max_len, is terminated by LF ('\n'), then padded to max_len with SP (' ')
*/
int get437StringLength(const uint8_t*str,int max_len){
    int len=max_len;
    while(len>0 && str[len-1]==' '){
        len--;
    }
    if(len>0 && str[len-1]=='\n'){
        len--;
    }
    return len;
}

// EDID is a widespread tech spec https://en.wikipedia.org/wiki/Extended_Display_Identification_Data
#define EDID_LENGTH 128
/* EDID must be at least 128 bytes long, but may be any larger multiple of 128, up to 32K */
void parse_edid(uint8_t edid[static EDID_LENGTH]){
    if(memcmp(edid,(uint8_t[8]){0x00,0xff,0xff,0xff,0xff,0xff,0xff,0x00},8)!=0){
        println("invalid edid header");
        return;
    }

    int edid_version=edid[18]<<8|edid[19];
    switch(edid_version){
        case 0x0103:
        case 0x0104:
        case 0x0200:
            break;
        default:
            println("unsupported edid version: %d",edid_version);
    }

    println("edid version: %d.%d",edid_version>>8,edid_version&0xff);

    char manufacturer[4];
    char monitor_name[14]={[13]=1};

    int manufatured_year=edid[17]+1990;
    println("model/manufactured year: %d",manufatured_year);

    manufacturer[0]=((edid[8]&0x7c)>>2)+'A'-1;
    manufacturer[1]=(((edid[8]&0x3)<<3)|((edid[9]&0xe0)>>5))+'A'-1;
    manufacturer[2]=(edid[9]&0x1F)+'A'-1;
    manufacturer[3]='\0';

    println("manufacturer: %s",manufacturer);

    for(int i=54;i<=124;i+=18){
        uint8_t *descriptor=&edid[i];

        int descriptor_index=(i-54)/18;
        if(descriptor_index==0){
            // first descriptor is "detailed timing descriptor"
            continue;
        }

        if(descriptor[0]==0x00 && descriptor[1]==0x00){
            // if the first two bytes are zero on a descriptor (except the first one), it is a monitor descriptor
            if(descriptor[2]!=0x00){
                // reserved, must be zero
                fatal("invalid monitor descriptor at index %d",descriptor_index);
            }

            // reserved on all descriptors (except range limits), must be zero
            if(descriptor[3]!=0xFD){
                if(descriptor[4]!=0x00)fatal("invalid monitor name descriptor at index %d",descriptor_index);
            }

            // ASCII text is "code page 437" text, i.e. is either 13 bytes long, if shorter terminated by LF, and padded by SP
            switch(descriptor[3]){
                // monitor serial number (ASCII text)
                case 0xFF:{
                    int text_len=get437StringLength(&descriptor[5],13);
                    println("serial number: %.*s",text_len,&descriptor[5]);
                    break;
                }
                // unspecified text (ASCII text)
                case 0xFE:{
                    int text_len=get437StringLength(&descriptor[5],13);
                    println("unspecified text: %.*s",text_len,&descriptor[5]);
                    break;
                }
                // monitor name
                case 0xFC:
                {
                    int text_len=get437StringLength(&descriptor[5],13);
                    memcpy(monitor_name,&descriptor[5],text_len);
                    // terminate string, and indicate (in this code) that a monitor name is present
                    monitor_name[13]=0;
                    break;
                }
                // EDID Display Range Limits Descriptor
                case 0xFD:{
                    break;
                }
                default:
                {
                    println("unknown monitor descriptor type: %x",descriptor[3]);
                    break;
                }
            }
        }
    }
    if(monitor_name[13]==1){
        println("monitor name not found in edid");
    }else{
        println("monitor name: %s",monitor_name);
    }

    int num_extensions=edid[126];
    println("num EDID extensions: %d",num_extensions);

    int checksum=0;
    for(int i=0;i<EDID_LENGTH;i++){
        checksum+=edid[i];
    }
    checksum%=256;
    if(checksum!=0){
        println("EDID checksum failed: %d",checksum);
    }

    for(int e=0;e<num_extensions;e++){
        int extension_tag=edid[(1+e)*128];
        switch(extension_tag){
            case 0x00: println("EDID Timing Extension block present"); break;
            case 0x02: println("EDID Additional Timing Data Block (CTA EDID Timing Extension) block present"); break;
            case 0x10: println("EDID Video Timing Block Extension (VTB-EXT) block present"); break;
            case 0x20: println("EDID EDID 2.0 Extension block present"); break;
            case 0x40: println("EDID Display Information Extension (DI-EXT) block present"); break;
            case 0x50: println("EDID Localized String Extension (LS-EXT) block present"); break;
            case 0x60: println("EDID Microdisplay Interface Extension (MI-EXT) block present"); break;
            case 0x70: println("EDID Display ID Extension block present"); break;
            case 0xA7:
            case 0xAF:
            case 0xBF:
                println("EDID Display Transfer Characteristics Data Block (DTCDB) block present");break;
            case 0xF0: println("EDID Block Map block present"); break;
            case 0xFF: println("EDID Display Device Data Block (DDDB) block present"); break;
            default:
                println("unknown EDID extension tag: %d",extension_tag);
        }
    }
}

#define VK_USE_PLATFORM_XCB_KHR
#include<vulkan/vulkan.h>

uint16_t width=800;
uint16_t height=600;
xcb_window_t window;
xcb_atom_t deleteAtom;

VkAllocationCallbacks* allocator=NULL;
VkInstance instance=VK_NULL_HANDLE;
VkSurfaceKHR surface=VK_NULL_HANDLE;
VkPhysicalDevice physical_device=VK_NULL_HANDLE;
VkDevice device=VK_NULL_HANDLE;
uint32_t queue_family_index=-1;
VkQueue queue=VK_NULL_HANDLE;
VkSwapchainKHR swapchain=VK_NULL_HANDLE;
VkCommandPool command_pool=VK_NULL_HANDLE;
#define MAX_NUM_SWAPCHAIN_FRAMES 4
uint32_t swapchainImageCount=0;
VkImage swapchain_images[MAX_NUM_SWAPCHAIN_FRAMES]={VK_NULL_HANDLE,VK_NULL_HANDLE,VK_NULL_HANDLE,VK_NULL_HANDLE};
VkFramebuffer framebuffers[MAX_NUM_SWAPCHAIN_FRAMES]={VK_NULL_HANDLE,VK_NULL_HANDLE,VK_NULL_HANDLE,VK_NULL_HANDLE};
VkImageView swapchain_image_views[MAX_NUM_SWAPCHAIN_FRAMES]={VK_NULL_HANDLE,VK_NULL_HANDLE,VK_NULL_HANDLE,VK_NULL_HANDLE};
VkCommandBuffer command_buffers[MAX_NUM_SWAPCHAIN_FRAMES]={VK_NULL_HANDLE,VK_NULL_HANDLE,VK_NULL_HANDLE,VK_NULL_HANDLE};
VkSemaphore image_available_semaphores[MAX_NUM_SWAPCHAIN_FRAMES]={VK_NULL_HANDLE,VK_NULL_HANDLE,VK_NULL_HANDLE,VK_NULL_HANDLE};
VkSemaphore render_finished_semaphores[MAX_NUM_SWAPCHAIN_FRAMES]={VK_NULL_HANDLE,VK_NULL_HANDLE,VK_NULL_HANDLE,VK_NULL_HANDLE};
VkRenderPass render_pass=VK_NULL_HANDLE;
VkShaderModule vertex_shader=VK_NULL_HANDLE,fragment_shader=VK_NULL_HANDLE;
VkPipelineLayout graphics_pipeline_layout=VK_NULL_HANDLE;
VkPipeline graphics_pipeline=VK_NULL_HANDLE;

static const VkImageSubresourceRange basicImageColorSubresourceRange=(VkImageSubresourceRange){
    .aspectMask=VK_IMAGE_ASPECT_COLOR_BIT,
    .baseMipLevel=0,
    .levelCount=1,
    .baseArrayLayer=0,
    .layerCount=1
};
static const VkSemaphoreCreateInfo semaphoreCreateInfo=(VkSemaphoreCreateInfo){
    .sType=VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    .pNext=NULL,
    .flags=0
};

void openWindow(void){
    println("doing xcb stuff");

	xcb_connection_t *x_conn=xcb_connect(NULL,NULL);
	if(!x_conn) fatal("could not connect to X server");

	const xcb_setup_t *x_setup=xcb_get_setup(x_conn);
	xcb_screen_iterator_t screen_iter=xcb_setup_roots_iterator(x_setup);
	for(int i=0;i<screen_iter.rem;i++){
		println("screen %d:",i);

		xcb_screen_t* screen=screen_iter.data;
		println("physical size: %umm x %umm",screen->width_in_millimeters,screen->height_in_millimeters);
		println("resolution: %u x %u",screen->width_in_pixels,screen->height_in_pixels);

		xcb_randr_query_version_cookie_t ver_cookie=xcb_randr_query_version(x_conn,1,5);
        xcb_randr_query_version_reply_t*ver_reply=xcb_randr_query_version_reply(x_conn,ver_cookie,NULL);
        if(!ver_reply)fatal("could not get randr version");

        xcb_randr_get_screen_resources_current_cookie_t res_cookie=xcb_randr_get_screen_resources_current(x_conn,screen->root);
        xcb_randr_get_screen_resources_current_reply_t *res_reply=xcb_randr_get_screen_resources_current_reply(x_conn,res_cookie,NULL);
        if(!res_reply)fatal("could not get screen resources");

        xcb_randr_output_t*outputs=xcb_randr_get_screen_resources_current_outputs(res_reply);
        int num_outputs=xcb_randr_get_screen_resources_current_outputs_length(res_reply);
        for(int i=0;i<num_outputs;i++){
            xcb_randr_get_output_info_cookie_t info_cookie=xcb_randr_get_output_info(x_conn,outputs[i],XCB_CURRENT_TIME);
            xcb_randr_get_output_info_reply_t*info_reply=xcb_randr_get_output_info_reply(x_conn,info_cookie,NULL);
            if(!info_reply)fatal("could not get output info");

            println("output %d name: %.*s",i, info_reply->name_len,xcb_randr_get_output_info_name(info_reply));

            xcb_randr_list_output_properties_cookie_t prop_cookie=xcb_randr_list_output_properties(x_conn,outputs[i]);
            xcb_randr_list_output_properties_reply_t *prop_reply=xcb_randr_list_output_properties_reply(x_conn,prop_cookie,NULL);
            if(!prop_reply)fatal("could not get output properties info");

            xcb_atom_t *props=xcb_randr_list_output_properties_atoms(prop_reply);
            int num_props=xcb_randr_list_output_properties_atoms_length(prop_reply);
            for(int j=0;j<num_props;j++){
                xcb_atom_t prop=props[j];

                xcb_get_atom_name_cookie_t name_cookie=xcb_get_atom_name(x_conn,prop);
                xcb_get_atom_name_reply_t *name_reply=xcb_get_atom_name_reply(x_conn,name_cookie,NULL);
                if(!name_reply)fatal("could not get atom name");

                int name_len=xcb_get_atom_name_name_length(name_reply);
                char*name=xcb_get_atom_name_name(name_reply);
                if(!(name_len==4 && strncmp(name,"EDID", 4)==0)){
                    // got some other atom that we ignore here, may be top/bottom margin, colorspace etc.
                    // println("got unexpected atom %.*s",name_len,name);
                    continue;
                }

                xcb_randr_get_output_property_cookie_t prop_value_cookie=xcb_randr_get_output_property(x_conn,outputs[i],prop,XCB_ATOM_NONE,0,100,0,0);
                xcb_randr_get_output_property_reply_t *prop_value_reply=xcb_randr_get_output_property_reply(x_conn,prop_value_cookie,NULL);
                if(!prop_value_reply)fatal("could not get output property value");

                // we expect EDID information to be at least EDID_LENGTH=128 bytes long
                if(prop_value_reply->type==XCB_ATOM_INTEGER && prop_value_reply->num_items >= EDID_LENGTH){
                    uint8_t*value=xcb_randr_get_output_property_data(prop_value_reply);
                    parse_edid(value);
                }
            }

            free(info_reply);
        }
        free(res_reply);
	}

	window=xcb_generate_id(x_conn);
    if(window<0) fatal("could not generate window id");

	int win_root=screen_iter.data->root;
	int16_t win_x=0,win_y=0;
	uint16_t win_class=XCB_WINDOW_CLASS_INPUT_OUTPUT;
	xcb_visualid_t win_visual=screen_iter.data->root_visual;
	uint32_t win_valmask=XCB_CW_BACK_PIXEL|XCB_CW_EVENT_MASK;
	uint32_t win_valist[2]={
		screen_iter.data->white_pixel,
		XCB_EVENT_MASK_KEY_PRESS | XCB_EVENT_MASK_KEY_RELEASE
        | XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE
        | XCB_EVENT_MASK_POINTER_MOTION
        | XCB_EVENT_MASK_ENTER_WINDOW | XCB_EVENT_MASK_LEAVE_WINDOW
        | XCB_EVENT_MASK_STRUCTURE_NOTIFY,
	};

	xcb_void_cookie_t win_create_cookie=xcb_create_window_checked(
		x_conn, 
		0,
		window,
		win_root,
		win_x, win_y, 
		width, height,
		0,
		win_class,
		win_visual,
		win_valmask,
		win_valist
	);
    check_cookie(x_conn,&win_create_cookie,"create window");

    // register window close event
    xcb_intern_atom_cookie_t cookie = xcb_intern_atom(x_conn, 1, strlen("WM_PROTOCOLS"), "WM_PROTOCOLS");
    xcb_intern_atom_reply_t* reply = xcb_intern_atom_reply(x_conn, cookie, NULL);
    if(!reply) fatal("could not intern atom WM_PROTOCOLS");

    xcb_intern_atom_cookie_t cookie2 = xcb_intern_atom(x_conn, 0, strlen("WM_DELETE_WINDOW"), "WM_DELETE_WINDOW");
    xcb_intern_atom_reply_t* reply2 = xcb_intern_atom_reply(x_conn, cookie2, NULL);
    if(!reply2) fatal("could not intern atom WM_DELETE_WINDOW");
    deleteAtom=reply2->atom;

    xcb_void_cookie_t change_property_cookie=xcb_change_property_checked(x_conn, XCB_PROP_MODE_REPLACE, window, (*reply).atom, 4, 32, 1, &(*reply2).atom);
    check_cookie(x_conn,&change_property_cookie,"change property");

    free(reply);
    free(reply2);

    // change window title
    const char*window_title="vulkan window";
    xcb_void_cookie_t change_title_cookie=xcb_change_property_checked(x_conn, XCB_PROP_MODE_REPLACE, window, XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 8, strlen(window_title), window_title);
    check_cookie(x_conn,&change_title_cookie,"change title");

	xcb_map_window(x_conn, window);

    println("done with xcb");

    println("doing vulkan stuff");

    VkResult res=VK_SUCCESS;

    int instanceExtensionCount=2;
    const char*instanceExtensions[2]={
        "VK_KHR_surface",
        "VK_KHR_xcb_surface"
    };
    int instanceLayerCount=1;
    const char*instanceLayers[1]={
        "VK_LAYER_KHRONOS_validation"
    };
	
    VkInstanceCreateInfo instanceCreateInfo=(VkInstanceCreateInfo){
        .sType=VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext=NULL,
        .flags=0,
        .enabledExtensionCount=instanceExtensionCount,
        .ppEnabledExtensionNames=instanceExtensions,
        .enabledLayerCount=instanceLayerCount,
        .ppEnabledLayerNames=instanceLayers,
    };
    res=vkCreateInstance(&instanceCreateInfo,allocator, &instance);
    if(res!=VK_SUCCESS) fatal("could not create vulkan instance: %d",res);

    uint32_t instanceLayerPropertyCount=0;
    vkEnumerateInstanceLayerProperties(&instanceLayerPropertyCount, NULL);
    VkLayerProperties*instanceLayerProperties=malloc(sizeof(VkLayerProperties[instanceLayerPropertyCount]));
    vkEnumerateInstanceLayerProperties(&instanceLayerPropertyCount, instanceLayerProperties);
    for(int i=-1;i<(int)instanceLayerPropertyCount;i++){
        const char*layername=NULL;
        if(i>=0){
            layername=instanceLayerProperties[i].layerName;
            println("instance layer: %s",layername);
        }

        uint32_t instanceExtensionPropertyCount=0;
        vkEnumerateInstanceExtensionProperties(layername, &instanceExtensionPropertyCount, NULL);
        VkExtensionProperties*instanceExtensionProperties=malloc(sizeof(VkExtensionProperties[instanceExtensionPropertyCount]));
        vkEnumerateInstanceExtensionProperties(layername, &instanceExtensionPropertyCount, instanceExtensionProperties);
        for(uint32_t i=0;i<instanceExtensionPropertyCount;i++){
            println("%sinstance extension: %s",layername?"  ":"",instanceExtensionProperties[i].extensionName);
        }
        free(instanceExtensionProperties);
    }
    free(instanceLayerProperties);

    VkXcbSurfaceCreateInfoKHR surfaceCreateInfo=(VkXcbSurfaceCreateInfoKHR){
        .sType=VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR,
        .pNext=NULL,
        .flags=0,
        .connection=x_conn,
        .window=window
    };
    res=vkCreateXcbSurfaceKHR(instance, &surfaceCreateInfo, allocator, &surface);
    if(res!=VK_SUCCESS) fatal("could not create vulkan surface: %d",res);

    uint32_t physicalDeviceCount=0;
    vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, NULL);
    VkPhysicalDevice *physicalDevices=malloc(sizeof(VkPhysicalDevice[physicalDeviceCount]));
    vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, physicalDevices);
    for(uint32_t i=0;i<physicalDeviceCount;i++){
        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(physicalDevices[i], &properties);
        println("physical device: %s",properties.deviceName);

        // print some more properties
        println("  api version: %d.%d.%d",VK_VERSION_MAJOR(properties.apiVersion),VK_VERSION_MINOR(properties.apiVersion),VK_VERSION_PATCH(properties.apiVersion));
        println("  driver version: %d",properties.driverVersion);
        println("  vendor id: %d",properties.vendorID);

        bool is_gpu=false;
        // also device type/group/kind
        switch(properties.deviceType){
            case VK_PHYSICAL_DEVICE_TYPE_OTHER:
                println("  device type: other");
                break;
            case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
                println("  device type: integrated gpu");
                is_gpu=true;
                break;
            case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
                println("  device type: discrete gpu");
                is_gpu=true;
                break;
            case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
                println("  device type: virtual gpu");
                is_gpu=true;
                break;
            case VK_PHYSICAL_DEVICE_TYPE_CPU:
                println("  device type: cpu");
                break;
            default:println("  device type: unknown");
        }

        if(!is_gpu){
            println("  - device is not a gpu, skipping");
            continue;
        }

        // enumerate device queue families
        uint32_t queueFamilyPropertyCount=0;
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevices[i], &queueFamilyPropertyCount, NULL);
        VkQueueFamilyProperties*queueFamilyProperties=malloc(sizeof(VkQueueFamilyProperties[queueFamilyPropertyCount]));
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevices[i], &queueFamilyPropertyCount, queueFamilyProperties);
        for(int j=0;j<(int)queueFamilyPropertyCount;j++){
            println("  queue family %d:",j);
            println("    queue count: %d",queueFamilyProperties[j].queueCount);
            println("    queue flags: %d",queueFamilyProperties[j].queueFlags);
            println("    timestamp valid bits: %d",queueFamilyProperties[j].timestampValidBits);
            println("    min image transfer granularity: %d %d %d",queueFamilyProperties[j].minImageTransferGranularity.width,queueFamilyProperties[j].minImageTransferGranularity.height,queueFamilyProperties[j].minImageTransferGranularity.depth);

            queue_family_index=j;

            // check if queue can present to window
            VkBool32 presentSupported=VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevices[i], j, surface, &presentSupported);
            if(presentSupported){
                println("    queue can present to window");
            }else fatal("    queue cannot present to window");
        }
        free(queueFamilyProperties);

        uint32_t deviceLayerCount=0;
        vkEnumerateDeviceLayerProperties(physicalDevices[i], &deviceLayerCount, NULL);
        VkLayerProperties*deviceLayerProperties=malloc(sizeof(VkLayerProperties[deviceLayerCount]));
        vkEnumerateDeviceLayerProperties(physicalDevices[i], &deviceLayerCount, deviceLayerProperties);
        for(int j=-1;j<(int)deviceLayerCount;j++){
            const char*layername=NULL;

            if(j>=0){
                layername=deviceLayerProperties[j].layerName;
                println("  device layer: %s",layername);
            }

            uint32_t deviceLayerCount=0;
            vkEnumerateDeviceExtensionProperties(physicalDevices[i], layername, &deviceLayerCount, NULL);
            VkExtensionProperties*deviceLayers=malloc(sizeof(VkExtensionProperties[deviceLayerCount]));
            vkEnumerateDeviceExtensionProperties(physicalDevices[i], layername, &deviceLayerCount, deviceLayers);
            for(uint32_t k=0;k<deviceLayerCount;k++){
                println("  %sdevice extension: %s",layername?"  ":"",deviceLayers[k].extensionName);
            }
            free(deviceLayers);
        }
        free(deviceLayerProperties);

        physical_device=physicalDevices[i];
    }
    if(physical_device==VK_NULL_HANDLE) fatal("no suitable physical device found");

    int numDeviceQueues=1;
    VkDeviceQueueCreateInfo deviceQueueCreateInfos[1]={
        (VkDeviceQueueCreateInfo){
            .sType=VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .pNext=NULL,
            .flags=0,
            .queueFamilyIndex=queue_family_index,
            .queueCount=1,
            .pQueuePriorities=(float[1]){1.0f}
        }
    };
    int numDeviceLayers=1;
    const char*deviceLayers[1]={
        "VK_LAYER_KHRONOS_validation",
    };
    int deviceExentsionCount=1;
    const char*deviceExtensions[1]={
        "VK_KHR_swapchain",
    };

    VkPhysicalDeviceFeatures deviceFeatures={};
    VkDeviceCreateInfo deviceCreateInfo=(VkDeviceCreateInfo){
        .sType=VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext=NULL,
        .flags=0,
        .queueCreateInfoCount=numDeviceQueues,
        .pQueueCreateInfos=deviceQueueCreateInfos,
        .enabledLayerCount=numDeviceLayers,
        .ppEnabledLayerNames=deviceLayers,
        .enabledExtensionCount=deviceExentsionCount,
        .ppEnabledExtensionNames=deviceExtensions,
        .pEnabledFeatures=&deviceFeatures,
    };
    res=vkCreateDevice(physical_device, &deviceCreateInfo, allocator, &device);
    if(res!=VK_SUCCESS) fatal("could not create vulkan device: %d",res);

    vkGetDeviceQueue(device, queue_family_index, 0, &queue);

    int swapchainQueueFamilyIndexCount=1;
    uint32_t swapchainQueueFamilyIndices[1]={queue_family_index};

    // get min swapchain image count
    VkSurfaceCapabilitiesKHR surfaceCapabilities={};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface, &surfaceCapabilities);
    int swapchainMinImageCount=surfaceCapabilities.minImageCount;

    VkExtent2D swapchainImageExtent=surfaceCapabilities.currentExtent;

    // get supported swapchain formats, present modes and colorspaces
    uint32_t swapchainFormatCount=0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &swapchainFormatCount, NULL);
    VkSurfaceFormatKHR*swapchainFormats=malloc(sizeof(VkSurfaceFormatKHR[swapchainFormatCount]));
    vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &swapchainFormatCount, swapchainFormats);

    VkSurfaceFormatKHR swapchainSurfaceFormat={.format=VK_FORMAT_UNDEFINED};
    for(uint32_t i=0;i<swapchainFormatCount;i++){
        println("swapchain format %d:",i);
        switch(swapchainFormats[i].format){
            case VK_FORMAT_B8G8R8A8_UNORM:
                println("  format: VK_FORMAT_B8G8R8A8_UNORM");
                break;
            case VK_FORMAT_B8G8R8A8_SRGB:
                println("  format: VK_FORMAT_B8G8R8A8_SRGB");
                // prefer this format
                if(swapchainSurfaceFormat.format==VK_FORMAT_UNDEFINED){
                    swapchainSurfaceFormat=swapchainFormats[i];
                }
                break;
            default:
                println("  format: unknown");
        }
        switch(swapchainFormats[i].colorSpace){
            case VK_COLOR_SPACE_SRGB_NONLINEAR_KHR:
                println("  color space: VK_COLOR_SPACE_SRGB_NONLINEAR_KHR");
                break;
            default:
                println("  color space: unknown");
        }
    }
    // fall back to first format found if none was selected based on preferences above
    if(swapchainSurfaceFormat.format==VK_FORMAT_UNDEFINED){
        swapchainSurfaceFormat=swapchainFormats[0];
    }
    free(swapchainFormats);

    VkSwapchainCreateInfoKHR swapchainCreateInfo=(VkSwapchainCreateInfoKHR){
        .sType=VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .pNext=NULL,
        .flags=0,
        .surface=surface,

        .minImageCount=swapchainMinImageCount,

        .imageFormat=swapchainSurfaceFormat.format,
        .imageColorSpace=swapchainSurfaceFormat.colorSpace,

        .imageExtent=swapchainImageExtent,
        .imageArrayLayers=1,

        .imageUsage=VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageSharingMode=VK_SHARING_MODE_EXCLUSIVE,

        .queueFamilyIndexCount=swapchainQueueFamilyIndexCount,
        .pQueueFamilyIndices=swapchainQueueFamilyIndices,

        .preTransform=VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
        .compositeAlpha=VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode=VK_PRESENT_MODE_FIFO_KHR,
        .clipped=VK_TRUE,

        .oldSwapchain=VK_NULL_HANDLE
    };
    res=vkCreateSwapchainKHR(device, &swapchainCreateInfo, allocator, &swapchain);
    if(res!=VK_SUCCESS) fatal("could not create swapchain: %d",res);

    vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, NULL);
    vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, swapchain_images);

    VkCommandPoolCreateInfo commandPoolCreateInfo=(VkCommandPoolCreateInfo){
        .sType=VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext=NULL,
        .flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex=queue_family_index
    };
    res=vkCreateCommandPool(device, &commandPoolCreateInfo, allocator, &command_pool);
    if(res!=VK_SUCCESS) fatal("could not create command pool: %d",res);

    VkRenderPassCreateInfo renderPassCreateInfo=(VkRenderPassCreateInfo){
        .sType=VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .pNext=NULL,
        .flags=0,
        .attachmentCount=1,
        .pAttachments=&(VkAttachmentDescription){
            .format=swapchainSurfaceFormat.format,
            .samples=VK_SAMPLE_COUNT_1_BIT,
            .loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp=VK_ATTACHMENT_STORE_OP_STORE,
            .stencilLoadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp=VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout=VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            .finalLayout=VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
        },
        .subpassCount=1,
        .pSubpasses=&(VkSubpassDescription){
            .pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS,
            .inputAttachmentCount=0,
            .pInputAttachments=NULL,
            .colorAttachmentCount=1,
            .pColorAttachments=&(VkAttachmentReference){
                .attachment=0,
                .layout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
            },
            .pResolveAttachments=NULL,
            .pDepthStencilAttachment=NULL,
            .preserveAttachmentCount=0,
            .pPreserveAttachments=NULL
        },
        .dependencyCount=0,
        .pDependencies=NULL
    };
    res=vkCreateRenderPass(device, &renderPassCreateInfo, allocator, &render_pass);
    if(res!=VK_SUCCESS) fatal("could not create render pass: %d",res);

    for(int i=0;i<(int)swapchainImageCount;i++){
        VkImageViewCreateInfo imageViewCreateInfo=(VkImageViewCreateInfo){
            .sType=VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .pNext=NULL,
            .flags=0,
            .image=swapchain_images[i],
            .viewType=VK_IMAGE_VIEW_TYPE_2D,
            .format=swapchainSurfaceFormat.format,
            .components={
                .r=VK_COMPONENT_SWIZZLE_IDENTITY,
                .g=VK_COMPONENT_SWIZZLE_IDENTITY,
                .b=VK_COMPONENT_SWIZZLE_IDENTITY,
                .a=VK_COMPONENT_SWIZZLE_IDENTITY
            },
            .subresourceRange=basicImageColorSubresourceRange,
        };
        res=vkCreateImageView(device, &imageViewCreateInfo, allocator, &swapchain_image_views[i]);
        if(res!=VK_SUCCESS) fatal("could not create image view: %d",res);

        res=vkCreateSemaphore(device, &semaphoreCreateInfo, allocator, &image_available_semaphores[i]);
        if(res!=VK_SUCCESS) fatal("could not create semaphore: %d",res);
        res=vkCreateSemaphore(device, &semaphoreCreateInfo, allocator, &render_finished_semaphores[i]);
        if(res!=VK_SUCCESS) fatal("could not create semaphore: %d",res);

        VkCommandBufferAllocateInfo commandBufferAllocateInfo=(VkCommandBufferAllocateInfo){
            .sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .pNext=NULL,
            .commandPool=command_pool,
            .level=VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount=1
        };
        res=vkAllocateCommandBuffers(device, &commandBufferAllocateInfo, &command_buffers[i]);
        if(res!=VK_SUCCESS) fatal("could not allocate command buffer: %d",res);

        VkFramebufferCreateInfo framebufferCreateInfo=(VkFramebufferCreateInfo){
            .sType=VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .pNext=NULL,
            .flags=0,
            .renderPass=render_pass,
            .attachmentCount=1,
            .pAttachments=&swapchain_image_views[i],
            .width=swapchainImageExtent.width,
            .height=swapchainImageExtent.height,
            .layers=1
        };
        res=vkCreateFramebuffer(device, &framebufferCreateInfo, allocator, &framebuffers[i]);
        if(res!=VK_SUCCESS) fatal("could not create framebuffer: %d",res);
    }


    VkPipelineLayoutCreateInfo graphicsPipelineLayoutCreateInfo=(VkPipelineLayoutCreateInfo){
        .sType=VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext=NULL,
        .flags=0,
        .setLayoutCount=0,
        .pSetLayouts=NULL,
        .pushConstantRangeCount=0,
        .pPushConstantRanges=NULL
    };
    res=vkCreatePipelineLayout(device, &graphicsPipelineLayoutCreateInfo, allocator, &graphics_pipeline_layout);
    if(res!=VK_SUCCESS) fatal("could not create graphics pipeline layout: %d",res);

    {
        int codeSize=0;
        uint32_t*code=malloc(4096);
        FILE*file=fopen("vert.spv","rb");
        if(!file)fatal("could not open vert.spv");
        /* read 1 item of sizeof(uint32_t) bytes per iteration, check if 1 item of this size was successfully read, then increase code size */
        while(fread(code+codeSize, sizeof(uint32_t), 1, file)==1)codeSize+=1;
        if(codeSize==0 || codeSize==4096) fatal("invalid code size %d",codeSize);
        VkShaderModuleCreateInfo shaderCreateInfo=(VkShaderModuleCreateInfo){
            .sType=VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .pNext=NULL,
            .flags=0,
            /* code size is in bytes, but we store codeSize as number of uint32_t sized items */
            .codeSize=codeSize*sizeof(uint32_t),
            .pCode=code
        };
        res=vkCreateShaderModule(device, &shaderCreateInfo, allocator, &vertex_shader);
        if(res!=VK_SUCCESS) fatal("could not create vertex shader module: %d",res);
        free(code);
    }
    {
        int codeSize=0;
        uint32_t*code=malloc(4096);
        FILE*file=fopen("frag.spv","rb");
        if(!file)fatal("could not open frag.spv");
        while(fread(code+codeSize, sizeof(uint32_t), 1, file)==1)codeSize+=1;
        if(codeSize==0 || codeSize==4096) fatal("invalid code size %d",codeSize);
        VkShaderModuleCreateInfo shaderCreateInfo=(VkShaderModuleCreateInfo){
            .sType=VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .pNext=NULL,
            .flags=0,
            .codeSize=codeSize*sizeof(uint32_t),
            .pCode=code
        };
        res=vkCreateShaderModule(device, &shaderCreateInfo, allocator, &fragment_shader);
        if(res!=VK_SUCCESS) fatal("could not create fragment shader module: %d",res);
        free(code);
    }

    VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo=(VkGraphicsPipelineCreateInfo){
        .sType=VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext=NULL,
        .flags=0,
        .stageCount=2,
        .pStages=(VkPipelineShaderStageCreateInfo[2]){
            (VkPipelineShaderStageCreateInfo){
                .sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .pNext=NULL,
                .flags=0,
                .stage=VK_SHADER_STAGE_VERTEX_BIT,
                .module=vertex_shader,
                .pName="main",
                .pSpecializationInfo=NULL
            },
            (VkPipelineShaderStageCreateInfo){
                .sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .pNext=NULL,
                .flags=0,
                .stage=VK_SHADER_STAGE_FRAGMENT_BIT,
                .module=fragment_shader,
                .pName="main",
                .pSpecializationInfo=NULL
            }
        },
        .pVertexInputState=&(VkPipelineVertexInputStateCreateInfo){
            .sType=VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
            .pNext=NULL,
            .flags=0,
            .vertexBindingDescriptionCount=0,
            .pVertexBindingDescriptions=NULL,
            .vertexAttributeDescriptionCount=0,
            .pVertexAttributeDescriptions=NULL
        },
        .pInputAssemblyState=&(VkPipelineInputAssemblyStateCreateInfo){
            .sType=VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
            .pNext=NULL,
            .flags=0,
            .topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
            .primitiveRestartEnable=VK_FALSE
        },
        .pTessellationState=NULL,
        .pViewportState=&(VkPipelineViewportStateCreateInfo){
            .sType=VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
            .pNext=NULL,
            .flags=0,
            .viewportCount=1,
            .pViewports=&(VkViewport){
                .x=0,
                .y=0,
                .width=swapchainImageExtent.width,
                .height=swapchainImageExtent.height,
                .minDepth=0,
                .maxDepth=1
            },
            .scissorCount=1,
            .pScissors=&(VkRect2D){
                .offset=(VkOffset2D){0,0},
                .extent=swapchainImageExtent
            }
        },
        .pRasterizationState=&(VkPipelineRasterizationStateCreateInfo){
            .sType=VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
            .pNext=NULL,
            .flags=0,
            .depthClampEnable=VK_FALSE,
            .rasterizerDiscardEnable=VK_FALSE,
            .polygonMode=VK_POLYGON_MODE_FILL,
            .cullMode=VK_CULL_MODE_BACK_BIT,
            .frontFace=VK_FRONT_FACE_COUNTER_CLOCKWISE,
            .depthBiasEnable=VK_FALSE,
            .depthBiasConstantFactor=0,
            .depthBiasClamp=0,
            .depthBiasSlopeFactor=0,
            .lineWidth=1
        },
        .pMultisampleState=&(VkPipelineMultisampleStateCreateInfo){
            .sType=VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
            .pNext=NULL,
            .flags=0,
            .rasterizationSamples=VK_SAMPLE_COUNT_1_BIT,
            .sampleShadingEnable=VK_FALSE,
            .minSampleShading=1.0,
            .pSampleMask=NULL,
            .alphaToCoverageEnable=VK_FALSE,
            .alphaToOneEnable=VK_FALSE
        },
        .pDepthStencilState=NULL,
        .pColorBlendState=&(VkPipelineColorBlendStateCreateInfo){
            .sType=VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            .pNext=NULL,
            .flags=0,
            .logicOpEnable=VK_FALSE,
            .logicOp=VK_LOGIC_OP_COPY,
            .attachmentCount=1,
            .pAttachments=&(VkPipelineColorBlendAttachmentState){
                .blendEnable=VK_FALSE,
                .srcColorBlendFactor=VK_BLEND_FACTOR_ONE,
                .dstColorBlendFactor=VK_BLEND_FACTOR_ZERO,
                .colorBlendOp=VK_BLEND_OP_ADD,
                .srcAlphaBlendFactor=VK_BLEND_FACTOR_ONE,
                .dstAlphaBlendFactor=VK_BLEND_FACTOR_ZERO,
                .alphaBlendOp=VK_BLEND_OP_ADD,
                .colorWriteMask=VK_COLOR_COMPONENT_R_BIT|VK_COLOR_COMPONENT_G_BIT|VK_COLOR_COMPONENT_B_BIT|VK_COLOR_COMPONENT_A_BIT
            },
            .blendConstants={0,0,0,0}
        },
        .pDynamicState=NULL,
        .layout=graphics_pipeline_layout,
        .renderPass=render_pass,
        .subpass=0,
        .basePipelineHandle=VK_NULL_HANDLE,
        .basePipelineIndex=-1
    };
    res=vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &graphicsPipelineCreateInfo, allocator, &graphics_pipeline);
    if(res!=VK_SUCCESS) fatal("could not create graphics pipeline: %d",res);

    println("done with vulkan");

    println("done with setup");

    fflush(stdout);

    bool stop_rendering=false;
    static const int fps=30;
    for(
        int frame_num=0;
        frame_num<300*fps && !stop_rendering;
        nanosleep(&(struct timespec){.tv_sec=0,.tv_nsec=(long)(1e9L/fps)},NULL),vkDeviceWaitIdle(device),frame_num++
    ){
        xcb_generic_event_t*event;
        while((event=xcb_poll_for_event(x_conn))){
            switch(event->response_type & ~0x80){
                case XCB_UNMAP_NOTIFY:
                    println("unmap notify event (window hidden)");
                    break;
                case XCB_MAP_NOTIFY:
                    println("map notify event (window un-hidden)");
                    break;
                case XCB_KEY_PRESS:
                    println("key press event");
                    break;
                case XCB_KEY_RELEASE:
                    println("key release event");
                    break;
                case XCB_BUTTON_PRESS:
                    println("button press event");
                    break;
                case XCB_BUTTON_RELEASE:
                    println("button release event");
                    break;
                case XCB_MOTION_NOTIFY:
                    println("motion notify event");
                    break;
                case XCB_ENTER_NOTIFY:
                    println("enter notify event");
                    break;
                case XCB_LEAVE_NOTIFY:
                    println("leave notify event");
                    break;
                case XCB_CLIENT_MESSAGE:
                    if((*(xcb_client_message_event_t*)event).data.data32[0] == deleteAtom){
                        println("window close event. stopping.");
                        stop_rendering=true;
                    }
                    break;
                default:
                    println("unknown event %d",event->response_type);
            }
            free(event);
        }
        if(stop_rendering)
            break;
        
        const int fi=frame_num%swapchainImageCount;
        const VkCommandBuffer cmdbuf=command_buffers[fi];

        uint32_t swapchainImageIndex=0;
        vkAcquireNextImageKHR(device, swapchain, 0, image_available_semaphores[0], VK_NULL_HANDLE, &swapchainImageIndex);
        VkImage swapchainImage=swapchain_images[swapchainImageIndex];
        VkFramebuffer framebuffer=framebuffers[swapchainImageIndex];

        // reset command buffer
        vkResetCommandBuffer(cmdbuf, 0);

        VkCommandBufferBeginInfo commandBufferBeginInfo=(VkCommandBufferBeginInfo){
            .sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .pNext=NULL,
            .flags=0,
            .pInheritanceInfo=NULL
        };
        vkBeginCommandBuffer(cmdbuf, &commandBufferBeginInfo);

        uint32_t memoryBarrierCount=0;
        VkMemoryBarrier *memoryBarriers=NULL;
        uint32_t bufferMemoryBarrierCount=0;
        VkBufferMemoryBarrier *bufferMemoryBarriers=NULL;
        uint32_t imageMemoryBarrierCount=1;
        VkImageMemoryBarrier imageMemoryBarriers[2]={
            /* acquire to clear */
            (VkImageMemoryBarrier){
                .sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                .pNext=NULL,
                .srcAccessMask=VK_ACCESS_MEMORY_READ_BIT,
                .dstAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                .oldLayout=VK_IMAGE_LAYOUT_UNDEFINED,
                .newLayout=VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                .srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
                .image=swapchainImage,
                .subresourceRange=basicImageColorSubresourceRange
            },
            /* clear to present */
            (VkImageMemoryBarrier){
                .sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                .pNext=NULL,
                .srcAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                .dstAccessMask=VK_ACCESS_MEMORY_READ_BIT,
                .oldLayout=VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                .newLayout=VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                .srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
                .image=swapchainImage,
                .subresourceRange=basicImageColorSubresourceRange
            }
        };
        vkCmdPipelineBarrier(
            cmdbuf,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            0,
            memoryBarrierCount, memoryBarriers,
            bufferMemoryBarrierCount, bufferMemoryBarriers,
            imageMemoryBarrierCount, imageMemoryBarriers
        );

        VkRenderPassBeginInfo renderPassBeginInfo=(VkRenderPassBeginInfo){
            .sType=VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .pNext=NULL,
            .renderPass=render_pass,
            .framebuffer=framebuffer,
            .renderArea=(VkRect2D){
                .offset=(VkOffset2D){0,0},
                .extent=swapchainImageExtent
            },
            .clearValueCount=1,
            .pClearValues=&(VkClearValue){
                .color=(VkClearColorValue){.float32={1.0,0,0,1}}
            }
        };
        vkCmdBeginRenderPass(cmdbuf, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, graphics_pipeline);
        vkCmdDraw(cmdbuf,3,1,0,0);
        vkCmdEndRenderPass(cmdbuf);
        vkCmdPipelineBarrier(
            cmdbuf,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            0,
            memoryBarrierCount, memoryBarriers,
            bufferMemoryBarrierCount, bufferMemoryBarriers,
            imageMemoryBarrierCount, &imageMemoryBarriers[1]
        );
        vkEndCommandBuffer(cmdbuf);

        VkSubmitInfo submitInfo=(VkSubmitInfo){
            .sType=VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .pNext=NULL,
            .waitSemaphoreCount=1,
            .pWaitSemaphores=&image_available_semaphores[0],
            .pWaitDstStageMask=(VkPipelineStageFlags[1]){VK_PIPELINE_STAGE_TRANSFER_BIT},
            .commandBufferCount=1,
            .pCommandBuffers=&cmdbuf,
            .signalSemaphoreCount=1,
            .pSignalSemaphores=&render_finished_semaphores[0]
        };
        vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);

        const VkPresentInfoKHR presentInfo=(VkPresentInfoKHR){
            .sType=VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .pNext=NULL,
            .waitSemaphoreCount=1,
            .pWaitSemaphores=&render_finished_semaphores[0],
            .swapchainCount=1,
            .pSwapchains=&swapchain,
            .pImageIndices=&swapchainImageIndex,
            .pResults=NULL
        };
        res=vkQueuePresentKHR(queue, &presentInfo);
        if(res!=VK_SUCCESS) fatal("could not present: %d",res);
    }

    println("rendering done. cleaning up.");

    vkDeviceWaitIdle(device);

    vkDestroyPipeline(device, graphics_pipeline, allocator);
    vkDestroyPipelineLayout(device, graphics_pipeline_layout, allocator);

    vkDestroyShaderModule(device, vertex_shader, allocator);
    vkDestroyShaderModule(device, fragment_shader, allocator);

    vkDestroyRenderPass(device, render_pass, allocator);

    for(int i=0;i<(int)swapchainImageCount;i++){
        vkDestroySemaphore(device, image_available_semaphores[i], allocator);
        vkDestroySemaphore(device, render_finished_semaphores[i], allocator);

        vkDestroyFramebuffer(device, framebuffers[i], allocator);
        vkDestroyImageView(device, swapchain_image_views[i], allocator);

        vkFreeCommandBuffers(device, command_pool, 1, &command_buffers[i]);
    }

    vkDestroyCommandPool(device,command_pool,allocator);
    vkDestroySwapchainKHR(device, swapchain, allocator);
    vkDestroyDevice(device, allocator);
    vkDestroySurfaceKHR(instance,surface,allocator);
    vkDestroyInstance(instance, allocator);

    println("cleaned up vulkan");

    xcb_unmap_window(x_conn, window);
    xcb_destroy_window(x_conn, window);
	xcb_disconnect(x_conn);

    println("cleaned up xcb");

    fflush(stdout);

    println("finished cleaning up. exiting.");
}
