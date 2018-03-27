#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "filesystem.h"
#include <QMainWindow>
#include <QCloseEvent>

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

private:
    void format(void);                              // 系统格式化
    void change_to_dir(void);                       // 进入文件夹
    int open_dir(int inode_id);                     // 进入文件夹
    void list_file(void);							// 显示当前目录下文件
    void create_file(void);							// 创建文件
    void create_dir(void);                          // 创建目录
    void del_file(void);                            // 删除文件
    void del_dir(void);                             // 删除目录
    void open_file(void);                           // 打开文件
    void close_file(void);                          // 关闭文件

    void del_file_by_index(int index);           //
    void del_dir_sub(int inode_id);

    int name_check(const char *, FileType type);  // 检查重名
    int get_inode(void);                            // 获取空闲inode
    int get_block(void);                            // 获取空闲block
    void save_cur_info(void);                       // 保存当前目录的inode和目录项表

    void closeEvent(QCloseEvent *event);

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

private slots:
    void on_mkfile_clicked();

    void on_mkdir_clicked();

    void on_getinto_dir_clicked();

    void on_delfile_clicked();

    void on_deldir_clicked();

    void on_openfile_clicked();

    void on_closefile_clicked();

private:
    Ui::MainWindow *ui;

    FILE *fp;   									// 模拟磁盘文件指针
    SuperBlock super_blk;
    vector<Dir> cur_dir_table;                      // 当前目录的目录项表
    int current_inode_idx;                          // 当前目录的inode编号
    Inode current_inode;							// 当前目录的inode内容
    int current_file_nums;  						// 当前目录下文件/目录个数

};

#endif // MAINWINDOW_H
