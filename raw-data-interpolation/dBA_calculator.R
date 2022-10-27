# Set working directory
setwd("C:/temp/sensors")

# Import packages
library("tidyverse")
library("ggplot2")
library("ggpubr")
library("lubridate")
library("readxl")

# Import data file from working directory
rawdata <- read.csv("datav2.txt", sep=";", header=FALSE)

rawdata <- rawdata %>%
  rename(
    date = V1,
    time = V2,
    lux = V3,
    IR = V4,
    full_light = V5,
    visible_light = V6,
    sigLeft = V7,
    freqLeft = V8,
    sigRight = V9,
    freqRight = V10,
    none = V11
  )

# Convert character into date
rawdata$datetime <- paste(rawdata$date, rawdata$time, sep=" ")

date_new <- dmy_hms(rawdata$datetime)
rawdata$datetime <- date_new

# Interpolate data
rawdata <- rawdata %>%
  mutate(dbaLeft = 0, dbaRight = 0) %>%
  select(datetime, lux, IR, full_light, visible_light, sigLeft, freqLeft, dbaLeft, sigRight, freqRight, dbaRight)

data <- read_excel("MAX9814_Messwerte.xlsx")
data <- as.matrix(data)

for (k in 1:nrow(rawdata)) {
  row <- 0
  col <- 0
  dBA <- 0
  for (i in 2:19) {
    if ((rawdata$freqLeft[k] > data[i,13]) & (rawdata$freqLeft[k] <= data[i+1,13])) {
      if (abs(rawdata$freqLeft[k]-data[i,13]) < abs(rawdata$freqLeft[k]-data[i+1,13])) {
        row <- i
        break
      } else row <- i+1
      break
    }
    i <- i+1
  }
  row
  for (j in 2:11) {
    if (rawdata$sigLeft[k] <= data[row,2]) {
      rawdata$dbaLeft[k] <- 30
      break
    }
    else if (rawdata$sigLeft[k] > data[row,12]) {
      rawdata$dbaLeft[k] <- 80
      break
    }
    else if ((rawdata$sigLeft[k] > data[row,j]) & (rawdata$sigLeft[k] <= data[row,j+1])) {
      rawdata$dbaLeft[k] <- (as.numeric(data[1,j]) + (rawdata$sigLeft[k] - as.numeric(data[row,j]))*(as.numeric(data[1,j]) - as.numeric(data[1,j+1]))/(as.numeric(data[row,j]) - as.numeric(data[row,j+1])))
      col <- j
      break
    }
    j <- j+1
  }
}

for (k in 1:nrow(rawdata)) {
  row <- 0
  col <- 0
  dBA <- 0
  for (i in 2:19) {
    if ((rawdata$freqRight[k] > data[i,13]) & (rawdata$freqRight[k] <= data[i+1,13])) {
      if (abs(rawdata$freqRight[k]-data[i,13]) < abs(rawdata$freqRight[k]-data[i+1,13])) {
        row <- i
        break
      } else row <- i+1
      break
    }
    i <- i+1
  }
  row
  for (j in 2:11) {
    if (rawdata$sigRight[k] <= data[row,2]) {
      rawdata$dbaRight[k] <- 30
      break
    }
    else if (rawdata$sigRight[k] > data[row,12]) {
      rawdata$dbaRight[k] <- 80
    }
    else if ((rawdata$sigRight[k] > data[row,j]) & (rawdata$sigRight[k] <= data[row,j+1])) {
      rawdata$dbaRight[k] <- (as.numeric(data[1,j]) + (rawdata$sigRight[k] - as.numeric(data[row,j]))*(as.numeric(data[1,j]) - as.numeric(data[1,j+1]))/(as.numeric(data[row,j]) - as.numeric(data[row,j+1])))
      col <- j
      break
    }
    j <- j+1
  }
}

rawdata$dbaLeft <- round(rawdata$dbaLeft,1)
rawdata$dbaRight <- round(rawdata$dbaRight,1)

# Write output to working directory
write.table(rawdata, file="datav2_interpolated.csv", sep=";", na=".", row.names=FALSE, col.names = FALSE)
