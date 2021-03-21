#include "node3d.h"
#include <math.h>
#include "constants.h"
#include "hybrid_astar.h"
#include <tf/transform_datatypes.h>
#include "visualize.h"
// #include "planner_core.h"
namespace hybrid_astar_planner {
    bool hybridAstar::calculatePath(const geometry_msgs::PoseStamped& start, const geometry_msgs::PoseStamped& goal,
                                        int cellsX, int cellsY, std::vector<geometry_msgs::PoseStamped>& plan ,ros::Publisher& pub, visualization_msgs::MarkerArray& pathNodes) {
        ROS_INFO("Using hybrid_astar mode!");
        std::cout << "the resolution of cost map " << costmap->getResolution() << std::endl;
        // 初始化优先队列，这里使用的是二项堆
        boost::heap::binomial_heap<Node3D*,boost::heap::compare<CompareNodes>> openSet;
        // 初始化并创建一些参数。
        // 创建charMap,charMap中存储的是地图的障碍物信息
        const unsigned char* charMap = costmap->getCharMap(); 
        float resolution = 0.125;//costmap->getResolution()
        unsigned int originalX, originalY, goalX, goalY;
        int cells_x,cells_y;
        cells_x = cellsX/2.5;
        cells_y = cellsY/2.5;
        int counter = 0;
        int dir;
        int iPred, iSucc;
        float t, g;
        unsigned int dx,dy;
        costmap->worldToMap(0, 0, dx, dy);
        dx = dx/2.5;
        dy = dy/2.5;
        // std::cout << "x : " << cells_x << " | y : " << cells_y << std::endl;
        // t为目标点朝向
        t = tf::getYaw(start.pose.orientation);
        Node3D* startPose = new Node3D(start.pose.position.x, start.pose.position.y, t , 0, 0, false, nullptr);
 
        t = tf::getYaw(goal.pose.orientation);
        Node3D* goalPose = new Node3D(goal.pose.position.x, goal.pose.position.y, t , 999, 0, false, nullptr);
        // updateH(*startPose, *goalPose, NULL, NULL, cells_x, cells_y, costmap);
        // return NULL;
        Node3D* pathNode3D = new Node3D[cells_x * cells_y * Constants::headings]();

        if (Constants::reverse) {
            dir = 6;
        }
        else {
            dir = 3;
        }

        openSet.push(startPose);
        pathNode3D[startPose->getindex(cells_x, Constants::headings, resolution, dx, dy)].setClosedSet();
        Node3D* tmpNode;
        Node3D* nSucc;
        while(openSet.size() && counter < Constants::iterations) {

            ++counter;
            // 根据混合A*算法，取堆顶的元素作为下查找节点
            tmpNode = openSet.top();
            // std::cout << "the G of node : " << tmpNode->getG() << "the F of node : " << tmpNode->getF() << std::endl;
            // std::cout << "x : " << tmpNode->getX() << " | y : " << tmpNode->getY() << std::endl;
            // publishSearchNodes(*tmpNode, pub, pathNodes,counter);
            openSet.pop();      //出栈
            if ( reachGoal(tmpNode, goalPose) ) {
                
                ROS_INFO("Got a plan,loop %d times!",counter);
                nodeToPlan(tmpNode, plan);
                
                delete [] pathNode3D;
                return true;
            }
            else {
                if (Constants::dubinsShot && tmpNode->isInRange(*goalPose) && !tmpNode->isReverse()) {
                nSucc = dubinsShot(*tmpNode, *goalPose, costmap);
                    /*
                    * dybinsShot方法之后再看
                    */
                    //如果Dubins方法能直接命中，即不需要进入Hybrid A*搜索了，直接返回结果
                    if (nSucc != nullptr && reachGoal(nSucc, goalPose) ) {//&& *nSucc == *goalPose
                        //DEBUG
                        // std::cout << "max diff " << max << std::endl;
                        delete [] pathNode3D;
                        nodeToPlan(nSucc, plan);
                        return true;//如果下一步是目标点，可以返回了
                    }
                }
            }
            // 拓展tmpNode临时点目标周围的点，并且使用STL标准库的向量链表进行存储拓展点Node3D的指针数据
            std::vector<Node3D*> adjacentNodes = gatAdjacentPoints(dir, cellsX, cellsY, charMap, pathNode3D, tmpNode);   
            // 将 tmpNode点在pathNode3D中映射的点加入闭集合中
            pathNode3D[tmpNode->getindex(cells_x, Constants::headings, resolution, dx, dy)].setClosedSet();
            for (std::vector<Node3D*>::iterator it = adjacentNodes.begin(); it != adjacentNodes.end(); ++it) {
                // 使用stl标准库中的interator迭代器遍历相邻点
                Node3D* point = *it;
                
                iPred = point->getindex(cells_x, Constants::headings, resolution, dx, dy);

                // 在pathNode3D集合里映射这个点
                pathNode3D[iPred].setX(point->getX());
                pathNode3D[iPred].setY(point->getY());
                pathNode3D[iPred].setT(point->getT());
                
                // std::cout << "push to binanalheap  :: " << iPred << std::endl;
                if (!pathNode3D[iPred].isClosedSet()) {
                    g = point->calcG();
                    if (!pathNode3D[iPred].isOpenSet() || (g < pathNode3D[iPred].getG())) {//
                        // point->setPerd(tmpNode);
                        // std::cout << "push to binanalheap" << std::endl;
                        point->setG(g);
                        pathNode3D[iPred].setG(g);
                        if(!pathNode3D[iPred].isOpenSet()) {
                            updateH(*point, *goalPose, NULL, NULL, cells_x, cells_y, costmap);
                            // point->calcH(goalPose);


                            point->setOpenSet();
                            


                            // 如果符合拓展点要求，则将此点加入优先队列中
                            openSet.push(point);
                        }
                        else {
                            openSet.push(point);
                        }

                    }
                }
            }
        }

        // goalPose->setPerd(startPose);
        // nodeToPlan(goalPose, plan);
        AstarInspiration(start, goal, plan, costmap, frame_id_);
        delete [] pathNode3D;
        return false;
    }


    std::vector<Node3D*> hybridAstar::gatAdjacentPoints(int dir, int cells_x, int cells_y, const unsigned char* charMap, Node3D* pathNode3D, Node3D *point) {
        std::vector<Node3D*> adjacentNodes;
        Node3D* tmpPtr;
        float xSucc;
        float ySucc;
        float tSucc;
        unsigned int startX, startY;
        float t = point->getT();
        // int index;
        float x = point->getX();
        float y = point->getY();
        unsigned int u32_x = int(x);
        unsigned int u32_y = int(y);
        for(int i = 0; i < dir; i++) {
            if (i < 3) {
                xSucc = x + Constants::dx[i] * cos(t) - Constants::dy[i] * sin(t);
                ySucc = y + Constants::dx[i] * sin(t) + Constants::dy[i] * cos(t);
            }
            else {
                xSucc = x - Constants::dx[i - 3] * cos(t) - Constants::dy[i - 3] * sin(t);
                ySucc = y - Constants::dx[i - 3] * sin(t) + Constants::dy[i - 3] * cos(t);
            }
            if( costmap->worldToMap(xSucc, ySucc, startX, startY)) {
                
                if (charMap[startX + startY * cells_x] < 253) {
                    // index = calcIndix(xSucc, ySucc, cells_x, t + Constants::dt[i]);
                    if (i<3) {
                        tmpPtr = new Node3D(xSucc, ySucc, t + Constants::dt[i], 999, 0, false,point);//+ 
                        tmpPtr->setCost(charMap[startX + startY * cells_x]);
                    }
                    else {
                        tmpPtr = new Node3D(xSucc, ySucc, t - Constants::dt[i-3], 999, 0, true,point);//point->getG()
                        tmpPtr->setCost(charMap[startX + startY * cells_x]);
                        // std::cout << "the node is reverse! " << std::endl;
                    }
                    adjacentNodes.push_back(tmpPtr);

                }
            }

        }

        return adjacentNodes;
    }

    bool hybridAstar::reachGoal(Node3D* node, Node3D* goalPose) {
        float nodeX = node->getX();
        float nodeY = node->getY();
        float goalX = goalPose->getX();
        float goalY = goalPose->getY();
        if ((nodeX < (goalX + point_accuracy) && nodeX > (goalX - point_accuracy)) && \
            (nodeY < (goalY + point_accuracy) && nodeY > (goalY - point_accuracy)) ) {
                if (node->getT()  < (goalPose->getT()+theta_accuracy )&& \
                    node->getT()  > (goalPose->getT()-theta_accuracy )) {
                        std::cout << node->getT() << "the T of goalPose" << goalPose->getT();
                        return true;
                }
                else {
                    std::cout << node->getT() << "the T of goalPose" << goalPose->getT();
                }
            }
        return false;
    }

    int hybridAstar::calcIndix(float x, float y, int cells_x, float t) {
        return (int(x) * cells_x + int(y)) * Constants::headings + int(t / Constants::headings);
    }

    void hybridAstar::nodeToPlan(Node3D* node, std::vector<geometry_msgs::PoseStamped>& plan) {
        Node3D* tmpPtr = node;
        geometry_msgs::PoseStamped tmpPose;
        std::vector<geometry_msgs::PoseStamped> replan;
        // float resolution = costmap->getResolution();
        unsigned int originalX,originalY;
        tmpPose.header.stamp = ros::Time::now();   
        //参数后期处理，发布到RViz上进行可视化
        while(tmpPtr!=nullptr) {
            tmpPose.pose.position.x = tmpPtr->getX();
            tmpPose.pose.position.y = tmpPtr->getY();
            tmpPose.header.frame_id = frame_id_;
            tmpPose.pose.orientation = tf::createQuaternionMsgFromYaw(tmpPtr->getT());
            replan.push_back(tmpPose);
            tmpPtr = tmpPtr->getPerd();

        }
        int size = replan.size();
        for (int i = 0;i < size; ++i) {
            plan.push_back(replan[size - i -1 ]);
        }

    }

}//end of namespace hybrid_astar_planner
