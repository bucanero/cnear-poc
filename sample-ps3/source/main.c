/*
   NEAR blockchain sample app / (C) 2024, Damian Parrino, <https://github.com/bucanero>.

   Based on TINY3D sample / (c) 2010 Hermes  <www.elotrolado.net>
*/

#include <sys/process.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/thread.h>
#include <sysmodule/sysmodule.h>
#include <sysutil/sysutil.h>
#include <net/net.h>

#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <math.h>

#include <cnear.h>
// Test signer account
#define TEST_ACCOUNT    "dev-1640093409715-22205231677544"
#define TEST_PUBKEY     "ed25519:FY835wAj7g8fRMncf4tqkyT3YdoW71t1ERnt3L78R28i"
#define TEST_PRVKEY     "ed25519:36gU64pAbTLH6uuxUeGvwF8n3fUfnDRSC87Z7Q5Ez2WdhcCy2KB6KtGX1WDcym6VezUhojWN4waBiwFAvxtXXNJN"

// Testnet smart contract
#define TEST_CONTRACT   "demo-devhub-vid102.testnet"

#include "gfx.h"
#include "pad.h"

#define RGBA(r, g, b, a) (((r) << 24) | ((g) << 16) | ((b) << 8) | (a))
#define ERROR(a, msg) { \
	if (a < 0) { \
		snprintf(msg_error, sizeof(msg_error), "(!) " msg ); \
        usleep(250); \
	} \
}

void release_all();

// thread
int running = 1;

volatile int flag_exit=0;

struct {
    int text;
    char name[MAXPATHLEN+1];
    char title[40+1];
} directories[256];

int menu_level    = 0;
int ndirectories  = 0;
int curdir = 0;

char msg_error[128];
char msg_two  [128];
char contract_msg[1024] = {0};
char user_input[1024];

u32 color_two = 0xffffffff;


static void call_contract(const char* usrmsg)
{
    char* greet_json;
    cnearResponse result;

    printf("------setting new message ('%s')------\n", usrmsg);
    asprintf(&greet_json, "{\"greeting\":\"%s\"}", usrmsg);

    result = near_contract_call_async(TEST_CONTRACT, "set_greeting", greet_json, NEAR_DEFAULT_100_TGAS, 0);
    if(result.rpc_code == 200)
    {
        printf("Response (%d):\n%s\n", result.rpc_code, result.json);
        contract_msg[0] = 0;
    }
    free(result.json);
    free(greet_json);
}

static void get_contract_greeting(void)
{
    cnearResponse result = near_rpc_call_function(TEST_CONTRACT, "get_greeting", "{}");
    if(result.rpc_code == 200)
    {
        size_t len;
        char* res = (char*) near_decode_result(&result, &len);

        if (res)
        {
            strncpy(contract_msg, res, sizeof(contract_msg));
            free(res);
        }

        printf("---\n%s\n---\n", contract_msg);
    }
    free(result.json);
}

static void control_thread(void* arg)
{
	int i;
    float x=0, y=0;
    static u32 C1, C2, C3, C4, count_frame = 0;
    
    int yesno =0;
	
	while(running) {
       
        ps3pad_read();

        if((new_pad & BUTTON_CIRCLE) && !menu_level){
			
            menu_level = 2; yesno = 0;
		}

        if(ndirectories <= 0 && (menu_level==1)) menu_level = 0;

        if((new_pad & BUTTON_CROSS)){

                switch(menu_level)
                {
                case 0:
                    if(ndirectories>0) {menu_level = 1; yesno = 0;}

                break;
                
                // set new greeting message
                case 1:

                    if(yesno) {

                        if (osk_dialog_get_text("Enter your message", user_input, sizeof(user_input)))
                            call_contract(user_input);

                        menu_level = 0;

                    } else menu_level = 0;

                break;
                
                // exit
                case 2:

                    if(yesno) {
                        flag_exit = 1;
                   } else menu_level = 0;

                break;
                }
		    }

         if((menu_level== 2 || menu_level== 1 || menu_level== 6)) {
            
            if((new_pad & BUTTON_LEFT))  yesno = 1;
            if((new_pad & BUTTON_RIGHT)) yesno = 0;

         }

        tiny3d_Clear(0xff000000, TINY3D_CLEAR_ALL);

        // Enable alpha Test
        tiny3d_AlphaTest(1, 0x10, TINY3D_ALPHA_FUNC_GEQUAL);

        // Enable alpha blending.
        tiny3d_BlendFunc(1, TINY3D_BLEND_FUNC_SRC_RGB_SRC_ALPHA | TINY3D_BLEND_FUNC_SRC_ALPHA_SRC_ALPHA,
            TINY3D_BLEND_FUNC_DST_RGB_ONE_MINUS_SRC_ALPHA | TINY3D_BLEND_FUNC_DST_ALPHA_ZERO,
            TINY3D_BLEND_RGB_FUNC_ADD | TINY3D_BLEND_ALPHA_FUNC_ADD);

        C1=RGBA(0, 0x30, 0x80 + (count_frame & 0x7f), 0xff);
        C2=RGBA(0x80 + (count_frame & 0x7f), 0, 0xff, 0xff);
        C3=RGBA(0,  (count_frame & 0x7f)/4, 0xff - (count_frame & 0x7f), 0xff);
        C4=RGBA(0, 0x80 - (count_frame & 0x7f), 0xff, 0xff);
        
        if(count_frame == 127) count_frame = 255;
        if(count_frame == 128) count_frame = 0;
        
        if(count_frame & 128) count_frame--;
        else count_frame++;

        tiny3d_SetPolygon(TINY3D_QUADS);

        tiny3d_VertexPos(0  , 0  , 65535);
        tiny3d_VertexColor(C1);

        tiny3d_VertexPos(847, 0  , 65535);
        tiny3d_VertexColor(C2);

        tiny3d_VertexPos(847, 511, 65535);
        tiny3d_VertexColor(C3);

        tiny3d_VertexPos(0  , 511, 65535);
        tiny3d_VertexColor(C4);

        tiny3d_End();
        update_twat();
    
        if(jpg1.bmp_out) {

            float x2=  ((float) ( (int)(count_frame & 127)- 63)) / 42.666f;
			
			// calculate coordinates for JPG
            x=(848-jpg1.width*2)/2; y=(512-jpg1.height*2)/2;

            tiny3d_SetTexture(0, jpg1_offset, jpg1.width, jpg1.height, jpg1.pitch, TINY3D_TEX_FORMAT_A8R8G8B8, 1);
            tiny3d_SetPolygon(TINY3D_QUADS);
            
            tiny3d_VertexPos(x + x2            , y +x2             , 65534);
            tiny3d_VertexColor(0xffffff10);
            tiny3d_VertexTexture(0.0f , 0.0f);

            tiny3d_VertexPos(x - x2 + jpg1.width*2, y +x2              , 65534);
            tiny3d_VertexTexture(0.99f, 0.0f);

            tiny3d_VertexPos(x + x2 + jpg1.width*2, y -x2+ jpg1.height*2, 65534);
            tiny3d_VertexTexture(0.99f, 0.99f);

            tiny3d_VertexPos(x - x2           , y -x2+ jpg1.height*2, 65534);
            tiny3d_VertexTexture(0.0f , 0.99f);

            tiny3d_End();
		}
 
        for(i = 0; i< 3; i++) {
            int index;
            float w, h, z = (i==1) ? 1 : 2;
            float scale = (i==1) ? 128 : 96;
            
            x=0; y=0;
            
            if(ndirectories > 0) index = ((u32) (ndirectories + curdir - 1 + i)) % ndirectories; else index = 0;

            // draw PNG
            if(ndirectories > 0 && directories[index].text >= 0 && Png_datas[directories[index].text].bmp_out) {
            
                x=(848 - scale * Png_datas[directories[index].text].width / Png_datas[directories[index].text].height) / 2 - 256 + 256 * i;
                y=(512- scale)/2;

                w = scale * Png_datas[directories[index].text].width / Png_datas[directories[index].text].height;
                h = scale;

                tiny3d_SetTexture(0, Png_offset[directories[index].text], Png_datas[directories[index].text].width,
                    Png_datas[directories[index].text].height, Png_datas[directories[index].text].pitch, TINY3D_TEX_FORMAT_A8R8G8B8, 1);

                if(directories[index].text == 4)
                    DrawTextBox(x, y, z, w, h, 0xffffff60);
                else
                    DrawTextBox(x, y, z, w, h, 0xffffffff);
            
            } else {

                x=(848 - scale * 2/1) / 2 - 256 + 256 * i;
                y=(512- scale)/2;

                w = scale * 2/1;
                h = scale;

                DrawBox(x, y, z, w, h, 0x80008060);
            
            }
        }

        SetFontSize(16, 32);
        x=0; y=0;
        
        SetFontColor(0xFFFF00FF, 0x00000000);
        SetFontAutoCenter(1);
        
        DrawString(x, y, "PS3 cNEAR Sample");
        
        SetFontAutoCenter(0);
        SetFontSize(12, 24);
        SetFontColor(0xFFFFFFFF, 0x00000000);

        y += 24 * 4;
        
        SetFontSize(24, 48);
        SetFontAutoCenter(1);

        DrawString(0, y, contract_msg[0] ? contract_msg : "loading message...");

        SetFontAutoCenter(0);
        SetFontSize(16, 32);

        if(ndirectories > 0) {

            SetFontAutoCenter(1);
            x= 0.0; y = 336;
            SetFontColor(0xFFFFFFFF, 0x80008050);

            if(directories[curdir].title[0] != 0)
                DrawFormatString(x, y, "%s", &directories[curdir].title[0]);
            else
                DrawFormatString(x, y, "%s", &directories[curdir].name[0]);

            SetFontAutoCenter(0);
        }

        
        SetFontSize(12, 24);
        SetFontAutoCenter(1);
        x= 0.0; y = 512 - 48;
        if(msg_error[0]!=0) {
            SetFontColor(0xFF3F00FF, 0x00000050);
            DrawFormatString(x, y, "%s", msg_error);
        }
        else {
            SetFontColor(color_two, 0x00000050);
            DrawFormatString(x, y, "%s", msg_two);
        }

        SetFontAutoCenter(0);
        SetFontColor(0xFFFFFFFF, 0x00000000);

        if(menu_level) {

            x= (848-640) / 2; y=(512-360)/2;

           // DrawBox(x, y, 0.0f, 640.0f, 360, 0x006fafe8);
            DrawBox(x - 16, y - 16, 0.0f, 640.0f + 32, 360 + 32, 0x00000038);
            DrawBox(x, y, 0.0f, 640.0f, 360, 0x300030d8);

            SetFontSize(16, 32);
            SetFontAutoCenter(1);
            
            y += 32;
            
            switch(menu_level) {
            
            case 1:
                DrawString(0, y, "Set a new Greeting?");
            break;
            
            case 2:
                DrawString(0, y, "Exit to XMB Menu?");
            break;

            }

            SetFontAutoCenter(0);
            
            y += 100;
            x = 300;

            DrawBox(x, y, 0.0f, 5*16, 32*3, 0x5f00afff);
            
            if(yesno) SetFontColor(0xFFFFFFFF, 0x00000000); else SetFontColor(0x606060FF, 0x00000000);
            
            DrawString(x+16, y+32, "YES");

            x = 848 - 300- 5*16;

            DrawBox(x, y, 0.0f, 5*16, 32*3, 0x5f00afff);
            if(!yesno) SetFontColor(0xFFFFFFFF, 0x00000000); else SetFontColor(0x606060FF, 0x00000000);
            
            DrawString(x+24, y+32, "NO");
        }

		sysThreadYield();
		
		tiny3d_Flip();
		sysUtilCheckCallback();
	}
	//you must call this, kthx
	sysThreadExit(0);
}

static void sys_callback(uint64_t status, uint64_t param, void* userdata) {

     switch (status) {
		case SYSUTIL_EXIT_GAME:
			release_all();
			sysProcessExit(1);
			break;
      
       default:
		   break;
         
	}
}

sys_ppu_thread_t thread1_id;

void release_all(void)
{
	u64 retval;

	sysUtilUnregisterCallback(SYSUTIL_EVENT_SLOT0);
	running= 0;
	sysThreadJoin(thread1_id, &retval);

    sysModuleUnload(SYSMODULE_JPGDEC);
    sysModuleUnload(SYSMODULE_PNGDEC);
    sysModuleUnload(SYSMODULE_NETCTL);
    sysModuleUnload(SYSMODULE_FS);

}


int main(int argc, const char* argv[], const char* envp[])
{
    sysModuleLoad(SYSMODULE_FS);
    sysModuleLoad(SYSMODULE_PNGDEC);
    sysModuleLoad(SYSMODULE_JPGDEC);
    sysModuleLoad(SYSMODULE_NETCTL);


	msg_error[0] = 0; // clear msg_error
    msg_two  [0] = 0;

    tiny3d_Init(1024*1024);
    
    LoadTexture();
    init_twat();

    ERROR(netInitialize(), "Error initializing network");

	ioPadInit(7);

    sysThreadCreate( &thread1_id, control_thread, 0ULL, 999, 256*1024, THREAD_JOINABLE, "Control Thread ps3load");

	// register exit callback
	sysUtilRegisterCallback(SYSUTIL_EVENT_SLOT0, sys_callback, NULL);

    ERROR(LoadTexturePNG("NEAR", 0), "Error loading texture");

    ndirectories = 1;
    directories[0].text = 0;
    strcpy(directories[0].title, "NEAR");

    if (!near_rpc_init(NEAR_TESTNET_RPC_SERVER_URL, true))
    {
        ERROR(-1, "Error initializing RPC");
    }

    if (!near_account_init(TEST_ACCOUNT, TEST_PRVKEY, TEST_PUBKEY))
    {
        ERROR(-1, "Error initializing account");
    }

    u32 check = 0;
    msg_two[0]   = 0;

	while (!flag_exit) {
		
        usleep(20000);

        if (check++ % 0x400 == 0) 
            get_contract_greeting();

        color_two = 0xffffffff;
		snprintf(msg_two, sizeof(msg_two), ".: press %s to set a new message :.", "(X)");
	}

    color_two = 0x00ff00ff;
	sprintf(msg_two, "Exiting...");
    usleep(250);
	release_all();

	sleep(2);
	return 0;
}
