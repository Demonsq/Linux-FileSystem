#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    fp(0)
{
    ui->setupUi(this);
    ui->getinput->setFocus();
    ui->textEdit->setReadOnly(true);
    ui->closefile->setEnabled(false);
    ui->textEdit->setLineWrapMode(QTextEdit::NoWrap);

    cur_dir_table.resize(DIRPERBLK);        // 目录表大小设置为最大值

    if((fp = fopen("disk.txt", "r+")) == 0){
        cout << "open failed" << endl;
        return;
    }

    // 读入根目录信息
    format();       // 初始化
    // 从模拟磁盘中读入超级块等信息
	fseek(fp, 0, SEEK_SET);
	if(fread(&super_blk, sizeof(SuperBlock), 1, fp) == 0) return;

	// 读取根目录的inode信息
	fseek(fp, INODE_BEGIN_POS, SEEK_SET);
	if(fread(&current_inode, sizeof(Inode), 1, fp) != 0)
        current_file_nums = current_inode.file_size / sizeof(Dir);

	// 初始默认目录为根目录
	// 读取目录表信息
	fseek(fp, BLOCK_BEGIN_POS, SEEK_SET);
    for(int i=0; i < current_file_nums; ++i)
        fread(&(cur_dir_table.at(i)), sizeof(Dir), 1, fp);

    list_file();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    // 保存超级块信息
    fseek(fp, 0, SEEK_SET);
    fwrite(&super_blk, sizeof(super_blk), 1, fp);

    // 保存当前目录inode和目录项信息
    save_cur_info();

    fclose(fp);

    event->accept();
}

// 格式化
void MainWindow::format()
{
    // 将0号inode分配给根目录
	memset(&super_blk, 0, sizeof(SuperBlock));
	super_blk.inode_bitmap[0] = true;
	super_blk.inode_used = 1;

	// 设置当前目录为根目录，编写目录项表
	current_inode_idx = 0;
	super_blk.block_bitmap[0] = true;
	super_blk.block_used = 1;

	memset(&current_inode, 0, sizeof(Inode));
	current_inode.type = Directory;
	current_inode.block_used = 1;
	current_inode.block_index[0] = 0;

	// 当前目录下仅有"."和".."两个目录项
	current_file_nums = 2;
	current_inode.file_size = current_file_nums * sizeof(Dir);
	cur_dir_table.at(0).linked_inode = 0;
	strcpy(cur_dir_table.at(0).name, ".");
	cur_dir_table.at(1).linked_inode = 0;
	strcpy(cur_dir_table.at(1).name, "..");

    list_file();
}

// 进入目录
void MainWindow::change_to_dir()
{
    QString file_name = ui->getinput->text();
    ui->getinput->clear();

    // 获取该目录/文件的inode
    int inode_id = -1;
    for(auto dir: cur_dir_table){
        if(strcmp(file_name.toStdString().c_str(), dir.name) == 0){
            inode_id = dir.linked_inode;
            break;
        }
    }
    // 文件/目录名不存在
    if(inode_id == -1){
        ui->statusBar->showMessage("Dir not exists.", 5000);
        return;
    }
    // 该文件不是目录
    if(open_dir(inode_id) == 0){
        ui->statusBar->showMessage(file_name + " is not a directory.", 5000);
        return;
    }

    list_file();
}

// 打开目录
int MainWindow::open_dir(int inode_id)
{
    // 判断该inode对应dir是否是目录
	Inode tmp;
	fseek(fp, INODE_BEGIN_POS + inode_id * sizeof(Inode), SEEK_SET);
	fread(&tmp, sizeof(Inode), 1, fp);
	if (tmp.type == File) return 0;

    // 保存当前目录的信息
    save_cur_info();

	// 读入该目录的inode信息
	memcpy(&current_inode, &tmp, sizeof(Inode));
	current_file_nums = current_inode.file_size / sizeof(Dir);					// 计算该目录下的文件数
	// 读入目录表信息
    cout << "current_file_nums = " << current_file_nums << endl;
    cout << "current_inode.block_index[0] = " << current_inode.block_index[0] << endl;
	fseek(fp, BLOCK_BEGIN_POS + current_inode.block_index[0] * BLOCK_SIZE, SEEK_SET);
    for(int i=0; i<current_file_nums; ++i)
        fread(&(cur_dir_table.at(i)), sizeof(Dir), 1, fp);

    return 1;
}

// 展示目录下文件
void MainWindow::list_file()
{
//    if (current_file_nums == 2) return;
    ui->textBrowser->clear();
	for (int i = 0; i < current_file_nums; ++i) {
        ui->textBrowser->append(cur_dir_table.at(i).name);
	}
    ui->getinput->setFocus();
}

// 创建文件
void MainWindow::create_file()
{
    // 暂时目录项最多只允许保存在一个block内
    if(current_file_nums >= DIRPERBLK){
        cout << "Dir table fulled.\n";
        return;
    }

    QString filename = ui->getinput->text();
    ui->getinput->clear();

    // 重名检查
    if (name_check(filename.toStdString().c_str(), File) != -1){
        ui->statusBar->showMessage("File exist.", 5000);
        return;
    }

	int inode_idx;
	// 设置目录项信息
	if ((inode_idx = get_inode()) == -1) return;
	cur_dir_table.at(current_file_nums).linked_inode = inode_idx;
	memset(cur_dir_table.at(current_file_nums).name, 0, FILENAME_LENGTH);
	strcpy(cur_dir_table.at(current_file_nums).name, filename.toStdString().c_str());
	++current_file_nums;				// 当前目录下文件数加一
    current_inode.file_size = current_file_nums*sizeof(Dir);

	// 更新超级块
	++super_blk.inode_used;
	super_blk.inode_bitmap[inode_idx] = true;

	// 设置该文件对应inode节点内容,并写回磁盘
	Inode tmp;
    memset(&tmp, 0, sizeof(Inode));
    tmp.type = File;
	fseek(fp, INODE_BEGIN_POS + inode_idx * sizeof(Inode), SEEK_SET);
	fwrite(&tmp, sizeof(Inode), 1, fp);

    list_file();
}

// 创建目录
void MainWindow::create_dir()
{
    // 暂时目录项最多只允许保存在一个block内
    if(current_file_nums >= DIRPERBLK){
        cout << "Dir table fulled.\n";
        return;
    }

    QString filename = ui->getinput->text();
    ui->getinput->clear();

    // 重名检查
	if (name_check(filename.toStdString().c_str(), Directory) != -1) return;

    // 获取空闲inode和block
	int inode_idx;
	int block_idx;
	if ((inode_idx = get_inode()) == -1) return;
	if ((block_idx = get_block()) == -1) return;
    // 设置目录项信息
	cur_dir_table.at(current_file_nums).linked_inode = inode_idx;
	memset(cur_dir_table.at(current_file_nums).name, 0, FILENAME_LENGTH);
	strcpy(cur_dir_table.at(current_file_nums).name, filename.toStdString().c_str());
	++current_file_nums;				// 当前目录下文件数加一
    current_inode.file_size = current_file_nums*sizeof(Dir);

	// 更新超级块
	++super_blk.inode_used;
	++super_blk.block_used;
	super_blk.inode_bitmap[inode_idx] = true;
	super_blk.inode_bitmap[block_idx] = true;

	// 设置该文件对应inode节点内容,并写回磁盘
    Inode tmp = {Directory, 2*sizeof(Dir), 1};
    memset(&tmp.block_index, 0, sizeof(tmp.block_index));
    tmp.block_index[0] = block_idx;
	fseek(fp, INODE_BEGIN_POS + inode_idx * sizeof(Inode), SEEK_SET);
	fwrite(&tmp, sizeof(Inode), 1, fp);

    Dir dir_tmp[2];
    memset(&dir_tmp, 0, sizeof(Dir)*2);
    // 新建的目录下仅有两个目录文件，"."和".."
    strcpy(dir_tmp[0].name, ".");
    dir_tmp[0].linked_inode = inode_idx;
    strcpy(dir_tmp[1].name, "..");
    dir_tmp[1].linked_inode = current_inode_idx;
    fseek(fp, BLOCK_BEGIN_POS + block_idx * BLOCK_SIZE, SEEK_SET);
    fwrite(dir_tmp, sizeof(Dir), 2, fp);

    list_file();
}

// 检查重名
int MainWindow::name_check(const char *name, FileType type)
{
    // 仅当在当前目录下文件名与类型均相同才判断为重名，返回true
	for (int i=0; i< current_file_nums; ++i) {
		if (strcmp(name, cur_dir_table.at(i).name) == 0) {
			Inode tmp;
			fseek(fp, INODE_BEGIN_POS + cur_dir_table.at(i).linked_inode * sizeof(Inode), SEEK_SET);
			fread(&tmp, sizeof(Inode), 1, fp);
			if (tmp.type == type) {
                    return i;
			}
		}
	}
	return -1;
}

// 删除文件
void MainWindow::del_file()
{
    int index;
    // 判断文件是否存在
    QString filename = ui->getinput->text();
    ui->getinput->clear();

    if((index = name_check(filename.toStdString().c_str(), File)) == -1){
        ui->statusBar->showMessage("File doesn't exist.", 5000);
        return;
    }
    del_file_by_index(index);
    list_file();
}

// 根据index删除对应内容
void MainWindow::del_file_by_index(int index)
{
    // 读取对应文件的inode
    int inode_id = cur_dir_table.at(index).linked_inode;
    Inode tmp;
    fseek(fp, INODE_BEGIN_POS + inode_id * sizeof(Inode), SEEK_SET);
    fread(&tmp, sizeof(Inode), 1, fp);
    // 更新super_blk中的block_used, inode_used, inode_bitmap[],
    for(int i=0; i<tmp.block_used; ++i){
        --super_blk.block_used;
        super_blk.block_bitmap[i] = false;
    }
    --super_blk.inode_used;
    super_blk.inode_bitmap[inode_id] = false;
    // 从当前文件目录表下删除该文件的表项
    vector<Dir>::iterator it = cur_dir_table.begin()+index;
    cur_dir_table.erase(it);
    cur_dir_table.resize(DIRPERBLK);
    --current_file_nums;
    current_inode.file_size = current_file_nums*sizeof(Dir);
}

// 删除目录
void MainWindow::del_dir()
{
    int index;
    // 判断目录是否存在
    QString filename = ui->getinput->text();
    ui->getinput->clear();

    if((index = name_check(filename.toStdString().c_str(), Directory)) == -1){
        ui->statusBar->showMessage("Directory doesn't exist.", 5000);
        return;
    }

    // 获取之前的得到的index
    int inode_id = cur_dir_table.at(index).linked_inode;
    if(inode_id == 0){
        ui->statusBar->showMessage("Can't delete root dir.", 5000);
        return;
    }
    del_dir_sub(index);
    list_file();
}

//
void MainWindow::del_dir_sub(int index)
{
    // 进入该目录
    int inode_id = cur_dir_table.at(index).linked_inode;
    open_dir(inode_id);

    int nums = current_file_nums;
    int indoe_tmp;
    for(int i=2; i<current_file_nums; ++i){
        // 如果是文件，则直接删除
        if(name_check(cur_dir_table.at(2).name, File) != -1){
            del_file_by_index(2);
            --current_file_nums;
        }
        // 如果是目录，则进入递归删除该目录
        else del_dir_sub(2);
    }
    // 如果当前目录为根目录则直接返回
    if(cur_dir_table.at(0).linked_inode == 0) return;
    // 回到上级目录，并删除本目录项
    open_dir(cur_dir_table.at(1).linked_inode);
    del_file_by_index(index);
}

// 打开文件
void MainWindow::open_file()
{
    int index;
    QString filename = ui->getinput->text();
    if((index = name_check(filename.toStdString().c_str(), File)) == -1){
        ui->statusBar->showMessage("File doesn't exist.", 5000);
        ui->getinput->clear();
        return;
    }
    ui->textEdit->setReadOnly(false);
    ui->getinput->setEnabled(false);
    ui->getinto_dir->setEnabled(false);
    ui->mkdir->setEnabled(false);
    ui->mkfile->setEnabled(false);
    ui->delfile->setEnabled(false);
    ui->deldir->setEnabled(false);
    ui->openfile->setEnabled(false);
    ui->closefile->setEnabled(true);

    // 获取该文件的inode
    Inode tmp;
    fseek(fp, INODE_BEGIN_POS + cur_dir_table.at(index).linked_inode * sizeof(Inode), SEEK_SET);
    fread(&tmp, sizeof(Inode), 1, fp);

    // 获取文件内容
    char buf[BLOCK_SIZE];
    memset(buf, 0, BLOCK_SIZE);
    QString text = buf;
    for(int i=0; i<tmp.block_used; ++i){
        fseek(fp, BLOCK_BEGIN_POS+tmp.block_index[i]*BLOCK_SIZE, SEEK_SET);
        fread(buf, 1, BLOCK_SIZE, fp);
        text += buf;
    }
    ui->textEdit->setText(text);
    ui->textEdit->setFocus();
}

// 关闭文件
void MainWindow::close_file()
{
    int index;
    QString filename = ui->getinput->text();
    if((index = name_check(filename.toStdString().c_str(), File)) == -1){
        ui->statusBar->showMessage("File doesn't exist.", 5000);
        ui->getinput->clear();
        return;
    }

    // 获取该文件的inode
    Inode tmp;
    fseek(fp, INODE_BEGIN_POS + cur_dir_table.at(index).linked_inode * sizeof(Inode), SEEK_SET);
    fread(&tmp, sizeof(Inode), 1, fp);

    // 获取文件内容
    QString text = ui->textEdit->toPlainText();     // 获取当前文件内容
    int block_nums = text.length()/BLOCK_SIZE+1;      // 计算所需blocks数

    // block不足时获取block
    int need = block_nums-tmp.block_used;
    int id;
    for(int i=0; i<need; ++i){
        if((id=get_block()) == -1);
        else{
            tmp.block_index[tmp.block_used] = id;
            ++tmp.block_used;
        }
    }

    // 内容写入block保存
    char buf[BLOCK_SIZE];
    for(int i=0; i<tmp.block_used; ++i){
        memset(buf, 0, BLOCK_SIZE);
        strcpy(buf, text.toStdString().c_str()+i*BLOCK_SIZE);
        fseek(fp, BLOCK_BEGIN_POS+tmp.block_index[i]*BLOCK_SIZE, SEEK_SET);
        fwrite(buf, BLOCK_SIZE, 1, fp);
    }

    // inode信息写回
    fseek(fp, INODE_BEGIN_POS + cur_dir_table.at(index).linked_inode * sizeof(Inode), SEEK_SET);
    fwrite(&tmp, sizeof(Inode), 1, fp);

    ui->textEdit->clear();
    ui->getinput->setFocus();
    ui->textEdit->setReadOnly(true);
    ui->getinput->setEnabled(true);
    ui->getinput->clear();
    ui->getinto_dir->setEnabled(true);
    ui->mkdir->setEnabled(true);
    ui->mkfile->setEnabled(true);
    ui->delfile->setEnabled(true);
    ui->deldir->setEnabled(true);
    ui->openfile->setEnabled(true);
    ui->closefile->setEnabled(false);
}

// 保存inode和dir_table信息
void MainWindow::save_cur_info()
{
    // 保存当前目录的inode信息
    fseek(fp, INODE_BEGIN_POS+sizeof(Inode)*current_inode_idx, SEEK_SET);
    fwrite(&current_inode, sizeof(Inode), 1, fp);

    // 保存当前目录的目录项信息
    fseek(fp, BLOCK_BEGIN_POS+current_inode.block_index[0]*BLOCK_SIZE, SEEK_SET);
    for(int i=0; i<current_file_nums; ++i)
        fwrite(&(cur_dir_table.at(i)), sizeof(Dir), 1, fp);
}

// 获取空闲inode
int MainWindow::get_inode()
{
    if (super_blk.inode_used == INODE_NUM) {
		ui->statusBar->showMessage("creat failed. run out of inodes.", 5000);
		return -1;
	}
	// 找到空闲inode块
	for (auto i = 0; i < INODE_NUM; ++i) {
		if (super_blk.inode_bitmap[i] == false)
			return i;
	}
}

// 获取空闲block
int MainWindow::get_block()
{
    if (super_blk.block_used == BLOCK_NUM) {
        ui->statusBar->showMessage("create failed. run out of blocks.", 5000);
		return -1;
	}
	// 找到空闲block块
	for (auto i = 0; i < BLOCK_NUM; ++i) {
		if (super_blk.block_bitmap[i] == false)
			return i;
	}
}

void MainWindow::on_mkfile_clicked()
{
    create_file();
}

void MainWindow::on_mkdir_clicked()
{
    create_dir();
}

void MainWindow::on_getinto_dir_clicked()
{
    change_to_dir();
}

void MainWindow::on_delfile_clicked()
{
    del_file();
}

void MainWindow::on_deldir_clicked()
{
    del_dir();
}

void MainWindow::on_openfile_clicked()
{
    open_file();
}

void MainWindow::on_closefile_clicked()
{
    close_file();
}
