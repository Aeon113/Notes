## 2018-09-11
### Gerrit 批量创建项目:
第一步:

首先把需要建立的库全部列在一个文件里面.这些库名字可以从源码下的`.repo/manifest.xml`中提取出来,然后建立一个`project-list`文件存放这个列表,内容如下:

project-list列表如下:
> platform_build   
> dplatform/abi/cpp   
> platform_bionic  
> platform/bootable/bootloader/legacy   
> platform_bootable_diskinstaller   
> platform/bootable/recovery   
> platform/cts   
> platform_dalvik   

............

第二步: 

写一个脚本来批量建立远端仓库,然后批量上传android源码.  假设这个脚本交`repo_creat_branch.sh`. 其内容如下:

######################
``` sh
for i in  `cat project-list`;   #这个list可以从manifest.xml中的name中提取出来
do
    echo $i     #测试用,加这里方便看进展
    ssh -p 29418 user@server_ip gerrit create-project -n project_name/$i;   #建立单个仓库
done
repo forall -c 'git push ssh://user@server_ip:29418/project_name/$REPO_PROJECT  HEAD:refs/heads/master'
##等循环建立玩各个仓库之后,用repo一键式上传所有代码,ok搞定.  这样完成之后你可以测试下是否成功的

##上传了所以代码,看如果下第三步:
```



第三步: repo init -u ssh://user@server_ip:29418/project_name/platform_manifest -b master 

第四步: repo sync   #这一步之后,如果代码全部下载下来,恭喜你,成功建立的自己的库.