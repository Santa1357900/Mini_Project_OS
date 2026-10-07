# Parallel File Search

## สมาชิกกลุ่ม

1. ณัฐพงศ์ กรธนกิจ  673380038-9
2. ธนกร ทองศรี  673380040-2
3. ธนันชัย พันธราช  673380042-8

โปรเจกต์ค้นหา **ชื่อไฟล์และโฟลเดอร์** แบบขนานบน Linux/POSIX ด้วยภาษา C แต่ละ worker เป็น process ที่สร้างด้วย `fork()` และรับผิดชอบรายการระดับแรกของโฟลเดอร์รากตามลำดับ modulo จำนวน worker จากนั้นค้นหาต่อแบบ recursive

## ความต้องการ

- Linux หรือระบบ POSIX ที่มี C11 compiler, `make`, และ shell สำหรับทดสอบ
- ไม่ต้องติดตั้ง library ภายนอก

## สร้างและใช้งาน

```sh
make
./psearch [-i] [-f] [-j WORKERS] ROOT PATTERN
./psearch -j 4 ~/Documents report
./psearch -i -j 2 . PDF
./psearch -f -i -j 4 /mnt/c/Users/USERNAME/Downloads report
./psearch -f -i -j 4 /mnt/d sqa
make test
```

`ROOT` ต้องเป็นโฟลเดอร์; `PATTERN` เป็นข้อความย่อยของ **ชื่อ** ไม่ใช่เนื้อหาไฟล์ ค่าเริ่มต้นคือ 4 workers; `-j` รับ 1–64; `-i` ค้นหาแบบไม่แยกตัวพิมพ์เฉพาะ ASCII; `-f` แสดงเฉพาะไฟล์ปกติ (ยังเดินผ่านโฟลเดอร์เพื่อค้นหาไฟล์ข้างใน) โปรแกรมแสดง path ที่พบหนึ่งรายการต่อบรรทัด และตามด้วย `Time: ... s` ซึ่งจับเวลาภายในโปรแกรมเอง โดย path ส่งออกทาง `stdout` และเวลาส่งออกทาง `stderr` ลำดับ path ขึ้นกับการอ่าน directory โฟลเดอร์ที่เข้าถึงไม่ได้จะถูกข้ามโดยไม่พิมพ์ `Permission denied`; ผลอาจไม่ครบทุกตำแหน่งที่ระบบปฏิเสธสิทธิ์ หากไม่มีผลลัพธ์ `stdout` จะว่างและ exit code เป็น 0; ข้อผิดพลาดสำคัญเป็น 1; argument ไม่ถูกต้องเป็น 2

โปรแกรมไม่ตาม symbolic link เพื่อป้องกันวงวน และใช้ `lstat()` จึงยังค้นหาชื่อ symbolic link ได้ หากมีชื่อไฟล์ที่มี newline ในชื่อ รูปแบบหนึ่ง path ต่อบรรทัดจะแยกความหมายไม่ได้

## โครงสร้างไฟล์

- `psearch.c` — ซอร์สโค้ด
- `Makefile` — build/test/clean
- `tests/test.sh` — ทดสอบผลการค้นหาและตัวเลือก
- `REPORT.md` — สถาปัตยกรรม POSIX calls ฟีเจอร์ และข้อจำกัด
- `HOW_IT_WORKS.md` — คำอธิบายการทำงานของโค้ดแบบละเอียด

## ตัวอย่าง demo

```sh
mkdir -p demo/reports demo/images
touch demo/reports/report-2026.txt demo/images/photo.png
./psearch -j 2 demo report
./psearch -i demo REPORT
```
