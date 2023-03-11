#include "WiFi.h"
#include "esp_camera.h"
#include "Arduino.h"
#include "soc/soc.h"           // Disable brownout problems
#include "soc/rtc_cntl_reg.h"  // Disable brownout problems
#include "driver/rtc_io.h"
#include <SPIFFS.h>
#include <FS.h>
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>

/*Nhập thông tin wifi*/
const char* ssid = "DucCoding";
const char* password = "20082k36";
// const char* ssid = "P407";
// const char* password = "17052000";

/*Thiết lập kết nối với firebase*/
#define API_KEY "AIzaSyBwmuegG8iA377Dz97NKz_9UOvhReVQJtk"
#define USER_EMAIL "m.ducdz2k@gmail.com"
#define USER_PASSWORD "Leduchandsome2008"
#define STORAGE_BUCKET_ID "smartlock-fc05a.appspot.com"

//#define API_KEY "AIzaSyDT1Aj8WLkVWGEJ9U61vRMRQSRz0eTgwbs"
//#define USER_EMAIL "minhducle2082k@gmail.com"
//#define USER_PASSWORD "123456789abc"
//#define STORAGE_BUCKET_ID "storageima-bc631.appspot.com"

/*Khai báo chân trigger nhận tín hiệu từ Client*/
int RECEIVED_PIN = 12;


// OV2640 camera module pins (CAMERA_MODEL_AI_THINKER)
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

boolean takeNewPhoto = false;
bool flagTake = true;
bool flagSave = false;
bool signupOK = false;

/*Định nghĩa data cho Firebase*/
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig configF;

/*Khai báo biến*/
bool taskCompleted = false;
bool checkUpload = true;
int countPath = 0;
char FILE_PHOTO[25];

/*Thay đổi đường dẫn lưu ảnh*/
void changeDefine (void)
{
  if(countPath == 0)
  {
    strcpy(FILE_PHOTO,"/data/image.jpg");
  }
  else if (countPath == 1)
  {
    strcpy(FILE_PHOTO,"/storage1/image.jpg");
  }
}

/*Kiểm tra trạng thái chụp ảnh*/
bool checkPhoto( fs::FS &fs ) {
  File f_pic = fs.open( FILE_PHOTO );
  unsigned int pic_sz = f_pic.size();
  return ( pic_sz > 100 );
}

/*Chụp ảnh và lưu ảnh*/
void capturePhotoSaveSpiffs( void ) {
  camera_fb_t * fb = NULL; // pointer
  bool ok = 0; // Boolean indicating if the picture has been taken correctly
  do {
    // Take a photo with the camera
    Serial.println("Taking a photo...");

    fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera capture failed");
      return;
    }
    // Photo file name
    Serial.printf("Picture file name: %s\n", FILE_PHOTO);
    File file = SPIFFS.open(FILE_PHOTO, FILE_WRITE);
    // Insert the data in the photo file
    if (!file) {
      Serial.println("Failed to open file in writing mode");
    }
    else {
      file.write(fb->buf, fb->len); // payload (image), payload length
      Serial.print("The picture has been saved in ");
      Serial.print(FILE_PHOTO);
      Serial.print(" - Size: ");
      Serial.print(file.size());
      Serial.println(" bytes");
    }
    // Close the file
    file.close();
    esp_camera_fb_return(fb);

    // check if file has been correctly saved in SPIFFS
    ok = checkPhoto(SPIFFS);
  } while ( !ok );
  Serial.print("Da chup anh xong \n");
}

/*Kết nối wifi*/
void initWiFi(){
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.println("Connected to WiFi. OK !!!");
  }
}

/*Khởi tạo Spiffs*/
void initSPIFFS()
{
  if (!SPIFFS.begin(true)) 
  {
    Serial.println("An Error has occurred while mounting SPIFFS");
    ESP.restart();
  }
  else {
    delay(500);
    Serial.println("SPIFFS mounted successfully");
  }
}

/*Khởi tạo camera*/
void initCamera(){
 // OV2640 camera module
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  if (psramFound()) {
    config.frame_size = FRAMESIZE_UXGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }

  // Camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) 
  {
    Serial.printf("Camera init failed with error 0x%x", err);
    ESP.restart();
  } 
  else
  {
    Serial.print("Camera init OK !!");
  }
}
/*Đặt điều kiện để thay đổi đường dẫn*/
void isChanged (void)
{
  changeDefine();
  delay(20);
  Serial.printf("%d: %s\n",countPath,FILE_PHOTO); /*Xuất trangj thái của đường dẫn hiện tại*/
  delay(500);
  countPath++;
  if (countPath > 1)
  {
    countPath = 0;
  }
}

void setup() 
{
  Serial.begin(9600);
  /*Khởi tạo chân và các chức năng của wifi & camera*/
  pinMode(RECEIVED_PIN,INPUT);
  initWiFi();
  initSPIFFS();
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
  initCamera();
  /*Đăng nhập vào Firebase*/ 
  configF.api_key = API_KEY;
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;
   
  if (Firebase.signUp(&configF, &auth, "", "")){
    Serial.println("ok");
    signupOK = true;
  }
  else{
    Serial.printf("%s\n", configF.signer.signupError.message.c_str());
  }
  configF.token_status_callback = tokenStatusCallback; //see addons/TokenHelper.h

  Firebase.begin(&configF, &auth);
  Firebase.reconnectWiFi(true);
}
void loop() 
{
  bool valueData = digitalRead(RECEIVED_PIN);
  /*Khi nhận giá trị High thì chụp ảnh*/ 
  if(valueData == 1)
  {
      // takeNewPhoto = true;
      Serial.println("Nhan gia tri HIGH");
      delay(10);
      valueData = 0;
      delay(500);
      isChanged();
      delay(500);
      Serial.print("Ready to take a photo\n");
      capturePhotoSaveSpiffs();
      delay(10);
      flagSave = true;
  }
  else
  {
    delay(1000);
    Serial.println("Nhan gia tri LOW");
  }
  delay(5);
  /*Tiến hành Upload ảnh lên Firebase*/ 
  if(flagSave)
  {
    if (Firebase.ready() && !taskCompleted)
    {
      // taskCompleted = true;
      Serial.print("Uploading ... ");
      if (Firebase.Storage.upload(&fbdo, STORAGE_BUCKET_ID, FILE_PHOTO, mem_storage_type_flash, FILE_PHOTO, "image/jpeg"))
      {
        Serial.print("\nUpload OK\n");
      }
      else
      {
        Serial.print("\nKhong upload duoc");
        Serial.println(fbdo.errorReason());
      }

      flagSave = false;
    }
  }
  
}
