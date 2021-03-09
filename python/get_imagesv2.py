
import urllib.request
from selenium import webdriver
from selenium.webdriver.support.ui import Select
from datetime import datetime
import time
from tqdm import tqdm

n_captures = 0
def get_time():
	now = datetime.now()
	current_time = now.strftime("%Y_%m_%d_%H_%M_%S")
	return current_time, now.strftime("%H")

def get_capture():
	global n_captures
	print("Capturando 20 imágenes ...")
	for i in range(20):
		button_element.click()
		now = datetime.now()
		current_time = now.strftime("%Y_%m_%d_%H_%M_%S")
		urllib.request.urlretrieve("http://192.168.43.203/capture?*", "./captures/{}.jpg".format(current_time))
		time.sleep(5)
		n_captures = 1
	print("Imágenes almacenadas!")
	print("A la espera de las proximas capturas ...")



def main():
	print("Capturas programadas para las 15:00")
	while(True):
		_, hour = get_time()
		if(hour =="15" and n_captures == 0):
			get_capture()
		elif(hour == "00"):
			n_captures = 0
		time.sleep(3)

if __name__ == "__main__":
	driver = webdriver.Firefox()

	driver.get('http://192.168.43.203/')
	select_resolution = Select(driver.find_element_by_xpath('//*[@id="framesize"]'))
	print("Configurando resolución de la cámara en 1280 x 1024 px")
	time.sleep(4)
	select_resolution.select_by_value('9')
	time.sleep(2)
	button_element = driver.find_element_by_id('get-still')
	main()



