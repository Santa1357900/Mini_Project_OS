# Final Report: Parallel File Search

## วัตถุประสงค์

ค้นหาไฟล์และโฟลเดอร์จากชื่อภายใต้ root directory โดยใช้หลาย process และ POSIX system calls แสดงผล path ที่ตรงกับข้อความที่กำหนด

## สถาปัตยกรรม

โปรแกรมหลักอ่านตัวเลือก ตรวจสอบ root และสร้าง worker ตาม `-j` (ค่าเริ่มต้น 4) ด้วย `fork()` Worker ทุกตัวอ่านรายการใน root และเลือกเฉพาะลำดับที่ `entry_index % worker_count == worker_index` จึงไม่มีรายการระดับแรกถูกค้นหาซ้ำ จากนั้นเดินต้นไม้ของรายการที่ได้รับแบบ depth first ด้วย `opendir()` / `readdir()` / `lstat()`

Worker เขียนผลลง anonymous temporary file ของตนเอง (`tmpfile()`) โปรแกรมหลักรอทุก worker ด้วย `waitpid()` แล้วอ่านไฟล์ผลลัพธ์ไป stdout วิธีนี้ป้องกัน pipe เต็มแล้ว deadlock และป้องกันข้อความจากหลาย process ปะปนกัน ข้อแลกเปลี่ยนคือผลลัพธ์แสดงหลัง worker เสร็จและใช้พื้นที่ temporary storage ตามขนาดผลลัพธ์

## POSIX calls และหน้าที่

| Call | หน้าที่ |
|---|---|
| `fork()` | สร้าง worker process |
| `waitpid()` | รอ worker และตรวจสถานะสำเร็จ |
| `opendir()`, `readdir()`, `closedir()` | อ่านรายการใน directory |
| `lstat()` | ตรวจชนิดไฟล์โดยไม่ตาม symbolic link |
| `stat()` | ตรวจสอบ root directory |
| `getopt()` | อ่านตัวเลือก command line |

`tmpfile()` และ `fread()`/`fwrite()` เป็น C/POSIX library APIs สำหรับเก็บและส่งต่อผลลัพธ์

## คุณสมบัติและข้อจำกัด

- ค้นหา substring ของชื่อไฟล์และ directory แบบ case sensitive หรือ ASCII case insensitive (`-i`)
- ใช้ `-f` เพื่อแสดงเฉพาะไฟล์ปกติ โดยยังค้นหาผ่านโฟลเดอร์ย่อย
- จำกัด worker 1–64 เพื่อไม่สร้าง process มากเกินไป
- ไม่ตาม symbolic link เพื่อหลีกเลี่ยง directory cycle
- งานแบ่งตามจำนวนรายการใน root ไม่ใช่ขนาด subtree: หาก subtree หนึ่งใหญ่เป็นพิเศษ อาจเกิด worker ที่ทำงานนานกว่า
- ชื่อที่มี newline ทำให้ output แบบหนึ่งบรรทัดต่อ path กำกวม
- การค้นหาไม่ใช่ snapshot: ไฟล์ที่ถูกเพิ่มหรือลบขณะทำงานอาจทำให้ผลเปลี่ยนหรือเกิด error
- `-i` รองรับเฉพาะ ASCII; ไม่มี Unicode case folding

## การทดสอบ

ใช้ `make test` สร้างต้นไม้ชั่วคราว เปรียบเทียบผลจาก 1 และ 4 workers ตรวจ case sensitivity, symbolic link cycle และ argument ที่ไม่ถูกต้อง
