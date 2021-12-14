#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"
#include <typeinfo>
#include <set>
#include <string>

#include "DopSeq.h"
#include "Utils.h"

using namespace llvm;

// CURRENT ISSUE: Need to add Phi nodes after the first DOP, in case there are
// reassignments of previous variables in either BB
// However, since the GDOPs always evaluate to the same value, we should be
// able to ignore these differences - we know that those values will not be
// used in the middle block, so it shouldn't be an issue

namespace {
    struct DopSeq : public FunctionPass {
        static char ID;
        bool flag = true;
        DopSeq() : FunctionPass(ID) {}
        DopSeq(bool flag) : FunctionPass(ID) {this->flag = flag; DopSeq();}

        bool runOnFunction(Function &F) override {
            errs() << "before to obf ";
            errs().write_escaped(F.getName()) << '\n';
            if(toObfuscate(flag,&F,"dopseq")) {
                //StringRef *sr = new StringRef("fun");
                //if (F.getName().equals(*sr)) {

                errs() << "Function: ";
                errs().write_escaped(F.getName()) << '\n';
                
                return addDopSeq(F);

                    // for (Function::iterator bb = F.begin(), e = F.end(); bb != e; ++bb) {
                    //     for (BasicBlock::iterator i = bb->begin(), e = bb->end(); i != e; ++i) {
                    //         errs().write_escaped(i->getOpcodeName()) << '\n';
                    //     }
                    // }
                // }
            }
            return false;
        }

        // add dynamic opaque predicate to sequential code
        bool addDopSeq(Function &F) {
            int numBB = 0;
            int numEdges = 0;
            for (Function::iterator bb = F.begin(); bb != F.end(); ++bb) {
                ++numBB;
                for (BasicBlock *Pred : predecessors(&(*bb))) {
                    ++numEdges;
                }
            }
            errs() << "original function:\nnum BBs = " << numBB << "\nnum edges = " 
                   << numEdges << "\ncyclomatic number = " << numEdges - numBB + 2 
                   << '\n';
            bool firstStore = true;
            unsigned int covar;
            BasicBlock *preBB, *postBB, *obfBB;
            BasicBlock::iterator preBBend, obfBBend, insertAlloca;
            Function::iterator bb_start;

            bool hasSetEnd = false;

            // split the snippet between two store instructions of a variable
            // the snippet is obfBB
            // TODO: FIX SO THAT THE STOREINST IS FROM THE SAME BB!
            // Current problem: the only time a StoreINst has the same opcode
            // is when the variable is global (because of phi nodes). So use
            // the valueID, since those should be the same thing (the ref to
            // store location of the variable)
            int tempNames = 500;
            std::set<string> nonStoreNames;

            for (Function::iterator bb = F.begin(), e = F.end(); bb != e; ++bb) {
                firstStore = true;
                covar = -1;
                string ogName = "";
                
                for (BasicBlock::iterator i = bb->begin(), e = bb->end(); i != e; ++i) {
                    unsigned opcode = i->getOpcode();
                    
                    // only make dops of Int32PtrTy (since that's most stores)
                    // and check to make sure we haven't tried this store already
                    if (opcode == Instruction::Store && 
                        i->getOperand(1)->getType() == Type::getInt32PtrTy(F.getContext())
                        && !nonStoreNames.count(ogName)) {
        
                        // errs() << "hello \n";
                        // errs() << i->getOperand(1)->getName() << "\n";
                        // errs() << *i->getOperand(1)->getType() << "\n";

                        if (firstStore == true &&
                            !nonStoreNames.count(string(i->getOperand(1)->getName()))) {
                            // errs() << *bb << "\n";
                            errs() << "The first store: " << *i << "\n";
                            //errs() << *i->getOperand(1)  << "\n";
                            
                            // create and assign a tempName to the operand. will be
                            // used to make sure we don't check the same store 2x
                            string tempN = "temp" + to_string(tempNames++);
                            Twine *tempName = new Twine(tempN);
                            i->getOperand(1)->setName(*tempName);

                            // set the covar (making sure they are storing to
                            // the same variable), ogName for later
                            covar = i->getOperand(1)->getValueID();
                            ogName = tempN;
                            insertAlloca = i;
                            preBBend = std::next(i);
                            bb_start = bb;
                            firstStore = false;
                            continue;
                        } else if (i->getOperand(1)->getValueID() == covar) {
                            obfBBend = i;

                            //make the sure obfBB is sufficiently long
                            int lenObfBB = 0;
                            for (BasicBlock::iterator b = preBBend, e = obfBBend; b != e; ++b)
                                lenObfBB++;

                            // if the space between the stores isn't sufficient, then
                            // add the value to the set (so we don't check it again),
                            if (lenObfBB + 1 < 4) {
                                nonStoreNames.insert(ogName);
                                --bb;
                            } else {
                                hasSetEnd = true;
                                //i->getOperand(1)->setName(ogName);
                                errs() << lenObfBB << "\n";
                                errs() << "The second store: " << *i << "\n";
                            }
                            break;
                        }
                    }
                    if (i == e) nonStoreNames.clear();
                }
                if (hasSetEnd) {
                    break;
                    // //make the sure obfBB is sufficiently long
                    // int lenObfBB = 0;
                    // for (BasicBlock::iterator b = preBBend, e = obfBBend; b != e; ++b)
                    //     lenObfBB++;

                    // //errs() << "size of obfBB = " << lenObfBB + 1 << "\n";
                    // if (lenObfBB + 1 < 4) hasSetEnd = false;
                    // else break;    
                }
            }
            // print out to see what load types we can have
            // for (std::set<Type>::iterator itr = loadInstTypes.begin(); itr != loadInstTypes.end(); ++itr) {
            //     errs() << *itr << "\n";
            // }
            //return false;
            if (!hasSetEnd) return false;

             
            //don't want this anymore as we don't know if it starts at the first BB
	        //Function::iterator bb = F.begin(); 
            preBB = &(*bb_start);
            //errs() << *preBB << "\n";
            Twine *var1 = new Twine("obfBB");
            // errs() << "gets to before obfBB assignment \n";
            obfBB = bb_start->splitBasicBlock(preBBend, *var1);
            // errs() << *obfBB << "hello \n";
            // errs() << *obfBBend << " hello \n";
            Twine *var2 = new Twine("postBB");
            //errs() << *obfBB << "hello \n";
            // this finds a store value that's inside the same BB
            postBB = obfBB->splitBasicBlock(obfBBend, *var2);

            // insert allca for the dop pointers
            BasicBlock::iterator ii = std::next(insertAlloca);
            // TODO Do we have to delete twines? Won't we have a memory leak otherwise?
            Twine *dop1_twine = new Twine("dop1"); //created new twine for allocainst
            Twine *dop2_twine = new Twine("dop2"); //created new twine for allocainst
            // use form (llvm::Type*, unsigned AddressSpace (deafult as 0), llvm::Twine&, llvm::Instruction*)
            //? What is F.getContext() ?
            AllocaInst* dop1 = new AllocaInst(Type::getInt32PtrTy(F.getContext()), 0, *dop1_twine, &(*ii)); //edited to work
            AllocaInst* dop2 = new AllocaInst(Type::getInt32PtrTy(F.getContext()), 0, *dop2_twine, &(*ii)); //edited t4o work
            // commented out these two lines below since the above instruction inserts these automatically
            // preBB->getInstList().insert(ii, dop1); 
            // preBB->getInstList().insert(ii, dop2);
            
            // store the variable's address to the dop pointers
            StoreInst* dop1st = new StoreInst(insertAlloca->getOperand(1), dop1, false, &(*ii));
            StoreInst* dop2st = new StoreInst(insertAlloca->getOperand(1), dop2, false, &(*ii));

            // errs() << *dop1st << '\n';

            // load the dop1's value
            // Twine *tmep1_twine = new Twine("temp1"); //created new twine for allocainst
            // Twine *temp2_twine = new Twine("temp2"); //created new twine for allocainst
            LoadInst* dop1p = new LoadInst(Type::getInt32PtrTy(F.getContext()), dop1, "", false, &(*ii));
            LoadInst* dop1deref = new LoadInst(Type::getInt32Ty(F.getContext()), dop1p, "", false, &(*ii));
            /*
             * LoadInst* dop1p = new LoadInst(dop1, "", false, 4, &(*ii)); // OLD
             * LoadInst* dop1deref = new LoadInst(dop1p, "", false, 4, &(*ii)); // OLD
             ? I think the 4 represents alignment? But I don't understand alginment at this point
             */

            // create alter BB from cloneing the obfBB
            const Twine & name = "clone";
            ValueToValueMapTy VMap;
            BasicBlock* alterBB = llvm::CloneBasicBlock(obfBB, VMap, name, &F);
            // errs() << *alterBB;
            // errs() << *obfBB;

            for (BasicBlock::iterator i = alterBB->begin(), e = alterBB->end() ; i != e; ++i) {
                // Loop over the operands of the instruction
                for(User::op_iterator opi = i->op_begin (), ope = i->op_end(); opi != ope; ++opi) {
                    // get the value for the operand
                    Value *v = MapValue(*opi, VMap,  RF_None, 0);
                    if (v != 0){
                        *opi = v;
                    }
                }
            }

            // Map instructions in obfBB and alterBB
	        std::map<Instruction*, Instruction*> fixssa;
            std::map<Instruction*, Instruction*> fixssa_reverse;
            for (BasicBlock::iterator i = obfBB->begin(), j = alterBB->begin(),
                                      e = obfBB->end(), f = alterBB->end(); i != e && j != f; ++i, ++j) {
	            // errs() << "install fix ssa:" << "\n";
	            fixssa[&(*i)] = &(*j);
                fixssa_reverse[&(*j)] = &(*i);
	            // fixssa[i] = j; // OLD
            }
            // for(auto i = fixssa.begin(); i != fixssa.end(); ++i){
            //     errs() << i->first << "  " << i->second << "\n";
            // }
            // Fix use values in alterBB
            
            for (BasicBlock::iterator i = alterBB->begin(), e = alterBB->end() ; i != e; ++i) {

                // errs() << "Instruction:\n";

                for (User::op_iterator opi = i->op_begin(), ope = i->op_end(); opi != ope; ++opi) {
                    
                    // Value *v = opi;

                    Instruction *vi = fixssa_reverse[dyn_cast<Instruction>(*opi)];
                    // errs() << vi << "\n";
                    // // errs() << typeid(vi).name() << "\n";
                    // errs() << **opi << "\n";
                    // errs() << typeid(**opi).name() << "\n";
                    // errs() << "\t" << vi << "\n";
                    if (fixssa.find(vi) != fixssa.end()) {
                        // errs() << "changing the instruction information \n";
                        // errs() << *opi << "\n";
                        *opi = (Value*)fixssa[vi];
                        // errs() << *opi << "\n";
                    }
                }
            }
            // errs() << *alterBB;
            // errs() << *obfBB;
            
            // for (std::map<Instruction*, Instruction*>::iterator it = fixssa.begin(), e = fixssa.end(); it != e; ++it) {
            //   errs() << "print fix ssa:" << "\n";
            //   errs() << "    " << it->first->getOpcodeName() << "\n";
            //   errs() << "    " << it->second->getOpcodeName() << "\n";
            // }

            // create the first dop at the end of preBB
            Twine * var3 = new Twine("dopbranch1");
            Value * rvalue = ConstantInt::get(Type::getInt32Ty(F.getContext()), 0);
            preBB->getTerminator()->eraseFromParent();
            ///////////////// TROUBLE LINE //////////////////////////
            // issue: trying to compare an i32 type (rvalue) with an i32* type (dop1deref)
            // don't know what the fix should be necessarily, but that's the problem
            // it looks like the main problem is that we are currently storing a pointer that
            // has no value... although I'm not really sure. Because we are trying to compare
            // a zero value to a pointer that's supposed to point to a DOP, but it doesn't do so...
            // it works for now by fixing the dop1deref variable to be a Int32 instead of a Int32Ptr,
            // but it actually working when run is a different thing.
            // errs() << "dop1p= " << *dop1p << "\n";
            // errs() << "dop1deref= " << *dop1deref << " \n";
            // errs() << "rvalue= " << *rvalue << " \n";
            // errs() << "dop1deref type= " << typeid((dop1deref->getOperand(0))).name() << " \n";
            // errs() << "rvalue type= " << typeid(rvalue).name() << " \n";
            ICmpInst * dopbranch1 = new ICmpInst(*preBB, CmpInst::ICMP_SGT, dop1deref, rvalue, *var3);
            //errs() << *dopbranch1 << "\n";

            //errs() << "gets past connecting BBs \n";
            BranchInst::Create(obfBB, alterBB, dopbranch1, preBB);
            
            // split the obfBB and alterBB with an offset
            // TODO Randomize this offset? need to know how big the BB chunks are
            BasicBlock::iterator splitpt1 = obfBB->begin(),
	                         splitpt2 = alterBB->begin();
            BasicBlock *obfBB2, *alterBB2;  
            int num = 2;
	        int n = num;
            for (BasicBlock::iterator e = obfBB->end(); splitpt1 != e && n > 0; ++splitpt1, --n) ;
	        n = num+1;
            for (BasicBlock::iterator e = alterBB->end(); splitpt2 != e && n > 0; ++splitpt2, --n) ;
            Twine *var4 = new Twine("obfBB2");
            //errs() << *obfBB << '\n';
            obfBB2 = obfBB->splitBasicBlock(splitpt1, *var4);
            Twine *var5 = new Twine("obfBBclone2");
            //errs() << *alterBB << "\n";

            // need to check if the BB is more than one instruction long
            // if it's not, then return false
            // int instCount = 0;
            // for (BasicBlock::iterator i = alterBB->begin(), e = alterBB->end();
            //      i != e; ++i)
            //      instCount++;
            // if (instCount < 2) return false;
            alterBB2 = alterBB->splitBasicBlock(splitpt2, *var5);
            //errs() << "hola hijo \n";

            // create the second dop as a separate BB
            // Twine *temp3_twine = new Twine("temp3");
            // Twine *temp4_twine = new Twine("temp4");
            BasicBlock* dop2BB = BasicBlock::Create(F.getContext(), "dop2BB", &F, obfBB2);

            LoadInst* dop2p = new LoadInst(Type::getInt32PtrTy(F.getContext()), dop2, "", false, &(*dop2BB));
            LoadInst* dop2deref = new LoadInst(Type::getInt32Ty(F.getContext()), dop2p, "", false, &(*dop2BB));
            // LoadInst* dop2p = new LoadInst(dop2, "", false, 4, dop2BB); // OLD
            // LoadInst* dop2deref = new LoadInst(dop2p, "", false, 4, dop2BB); // OLD

            Twine * var6 = new Twine("dopbranch2");
            Value * rvalue2 = ConstantInt::get(Type::getInt32Ty(F.getContext()), 0);
            // dop2BB->getTerminator()->eraseFromParent();
            ICmpInst * dopbranch2 = new ICmpInst(*dop2BB, CmpInst::ICMP_SGT , dop2deref, rvalue, *var3);
            BranchInst::Create(obfBB2, alterBB2, dopbranch2, dop2BB);
            
            // connect obfBB and alterBB to the second dop
            obfBB->getTerminator()->eraseFromParent();
            BranchInst::Create(dop2BB, obfBB);
            alterBB->getTerminator()->eraseFromParent();
            BranchInst::Create(dop2BB, alterBB);

                     
            
            // Iterate over alterBB2, finding uses of values defined in obfBB or obfBB2
             // The target at which to insert PHINodes
            std::map<Instruction*, PHINode*> dop2BBPHI; // instructions to PHI nodes
            for (BasicBlock::iterator i = obfBB2->begin(), e = obfBB2->end() ; i != e; ++i) {
                for(User::op_iterator opi = i->op_begin(), ope = i->op_end(); opi != ope; ++opi) {
                    Instruction *def = dyn_cast<Instruction>(*opi);

                    // If the opi can't be cast to an Instruction*
                    if(def == nullptr) continue;

                    /*
                     *  If definition is in obfBB
                     *  Then insert PHI node in dop2BB, replace uses after it with the Phi node value,
                     *  No cleanup phinode necessary in postBB, because the value is necessarily defined there
                    */
                    if (def->getParent() == obfBB ) {

                        // If PHINode not already created for this Value
                        if(auto i_phi = dop2BBPHI.find(def); i_phi == dop2BBPHI.end()) {
                            // Insert PhiNode into dop2BB
                            PHINode *phi;
                            ii = dop2BB->begin();
                            phi = PHINode::Create(def->getType(), 2, "", &(*ii));
                            phi->addIncoming(def, def->getParent());
                            phi->addIncoming(fixssa[def], fixssa[def]->getParent());
                            dop2BBPHI[def] = phi;
                        }

                        // Replace operand in obfBB2 and alterBB2 with the Value of the PHINode
                        *opi = (Value*) dop2BBPHI[def];
                        if(fixssa[&(*i)]->getParent() != alterBB) {
                            fixssa[&(*i)]->setOperand(opi->getOperandNo(), (Value*) dop2BBPHI[def]);
                        }
                        
                    }
                    /*
                     *  Else if defined in obfBB2 in one path and alterBB on the other path
                     *  Then insert dummy into obfBB, insert phinode into dop2BB, replace uses in alterBB2 only
                     *  with the phinode value, and add cleanup phinode in postBB (regular value on left, 
                     *  phinode value on right)                 
                    */   
                    else if (def->getParent() == obfBB2 && fixssa[def]->getParent() == alterBB) {

                        // If PHINode not already created for this Value
                        if(auto i_phi = dop2BBPHI.find(def); i_phi == dop2BBPHI.end()) {
                            // Insert dummy instruction at the end of obfBB
                            // TODO make this instruction just set a constant value or something
                            // Instruction *dummy_inst = obfBB2->begin()->clone();
                            // Instruction *dummy_inst = new LoadInst(def->getType(), (Value *)ConstantPointerNull::get(PointerType::get(def->getType(), 0)), "", &(*obfBB->begin()));
                            //obfBB->getInstList().insert(obfBB->begin(), dummy_inst);

                            // Insert PHI Node into dop2BB
                            // create phi node to insert at beginning of dopBB2
                            ii = dop2BB->begin();
                            PHINode *phi;
                            phi = PHINode::Create(def->getType(), 2, "", &(*ii));
                            phi->addIncoming(Constant::getNullValue(def->getType()), obfBB);
                            phi->addIncoming(fixssa[def], alterBB);

                            // Add cleanup phiNode in postBB
                            ii = postBB->begin();
                            PHINode *phi2;
                            phi2 = PHINode::Create(def->getType(), 2, "", &(*ii));
                            phi2->addIncoming( (Value*) def, obfBB2);
                            phi2->addIncoming( (Value*) phi, alterBB2);

                            // Add to map
                            dop2BBPHI[def] = phi;
                            dop2BBPHI[(Instruction *)phi] = phi2;
                        }
                        
                        // Replace operand in alterBB2 ONLY with the Value of the PHINode
                        fixssa[&(*i)]->setOperand(opi->getOperandNo(), (Value*) dop2BBPHI[def]);
                    }
                }
            }

            // insert phi node and update uses in postBB
            ii = postBB->begin();
            for (BasicBlock::iterator i = postBB->begin(), e = postBB->end() ; i != e; ++i) {
                // if it's a phi node, that means we've inserted it. keep going
                if (isa<PHINode>(*i)){
                    errs() << "opname: " << i->getOpcodeName() << '\n';
                    continue;
                }
                for(User::op_iterator opi = i->op_begin(), ope = i->op_end(); opi != ope; ++opi) {
                    Instruction *p;
                    Instruction *def = dyn_cast<Instruction>(*opi);
		            // PHINode *q;

                    // If the opi can't be cast to an Instruction*
                    if (def == nullptr) continue;

                    // if (auto i_phi = dop2BBPHI.find(def); i_phi == dop2BBPHI.end())
                    //     *opi = (Value*) dop2BBPHI[def];
                    //     continue;

                    // If both possible operands come from obfBB and alterBB, can makea phi node in dop2BB
                    if (def->getParent() == obfBB && fixssa[def]->getParent() == alterBB) {
                        if (auto i_phi = dop2BBPHI.find(def); i_phi == dop2BBPHI.end()) {
                            PHINode *phi;
                            ii = dop2BB->begin();
                            phi = PHINode::Create(def->getType(), 2, "", &(*ii));
                            phi->addIncoming(def, def->getParent());
                            phi->addIncoming(fixssa[def], fixssa[def]->getParent());
                            dop2BBPHI[def] = phi;
                        }

                        //set operand for postBB use
                        errs() << "Case 1: swapping out operand " << *opi << "\n";
                        *opi = (Value*) dop2BBPHI[def];
                        errs() << "operand is now " << *opi << "\n";
                    }
                    else if (def->getParent() == obfBB2 && fixssa[def]->getParent() == alterBB2) {
                        if (auto i_phi = dop2BBPHI.find(def); i_phi == dop2BBPHI.end()) {
                            PHINode *phi;
                            ii = postBB->begin();
                            phi = PHINode::Create(def->getType(), 2, "", &(*ii));
                            phi->addIncoming(def, def->getParent());
                            phi->addIncoming(fixssa[def], fixssa[def]->getParent());
                            dop2BBPHI[def] = phi;
                        }                        

                        //set operand for postBB use
                        errs() << "Case 2: swapping out operand " << *opi << "\n";
                        *opi = (Value*) dop2BBPHI[def];
                        errs() << "operand is now " << *opi << "\n";
                    }
                    else if (def->getParent() == obfBB2 && fixssa[def]->getParent() == alterBB) {
                        if (auto i_phi = dop2BBPHI.find(def); i_phi == dop2BBPHI.end() && dop2BBPHI.find((Instruction *)&(*i_phi)) == dop2BBPHI.end()) {
                            //Instruction *dummy_inst = new LoadInst(def->getType(), (Value *)ConstantPointerNull::get(PointerType::get(def->getType(), 0)), "", &(*obfBB->begin()));
                            
                            //obfBB->getInstList().insert(obfBB->begin(), dummy_inst);
                            // Insert PHI Node into dop2BB
                            // create phi node to insert at beginning of dopBB2
                            ii = dop2BB->begin();
                            PHINode *phi;
                            phi = PHINode::Create(def->getType(), 2, "", &(*ii));
                            phi->addIncoming(Constant::getNullValue(def->getType()), obfBB);
                            phi->addIncoming(fixssa[def], alterBB);

                            // Add cleanup phiNode in postBB
                            ii = postBB->begin();
                            PHINode *phi2;
                            phi2 = PHINode::Create(def->getType(), 2, "", &(*ii));
                            phi2->addIncoming( (Value*) def, obfBB2);
                            phi2->addIncoming( (Value*) phi, alterBB2);

                            // Add to map
                            dop2BBPHI[def] = phi;
                            dop2BBPHI[(Instruction *)phi] = phi2;
                        }

                        errs() << "Special case: swapping out operand " << *opi << "\n";
                        *opi = (Value*) dop2BBPHI[dop2BBPHI[def]];
                        errs() << "operand is now " << *opi << "\n";
                    }

                    // if (fixssa.find(def) != fixssa.end()) {
                    //     PHINode *fixnode;
                    //     p = fixssa[def];
                    //     if (dop2BBPHI.find(def) == dop2BBPHI.end()) {
                    //         q = dop2BBPHI[vi];
                    //         // Twine *temp5_twine = new Twine("temp5");
                    //         fixnode = PHINode::Create(vi->getType(), 2, "", &(*ii));
                    //         fixnode->addIncoming(vi, vi->getParent());
                    //         fixnode->addIncoming(p, p->getParent());
                    //         dop2BBPHI[vi] = fixnode;
                    //     } else {
                    //         fixnode = q;
                    //     }
                    //     *opi = (Value*)fixnode;
                    // }
                }
            }
            
            // special case: check to see if the extra operand in the cloned BB
            // is an instruction that's used on the next line.
            // if it is, then insert a phi node with a junk left value in dop2BB
            // and insert another phi node in postBB, then fix down
            // if ((----alterBB->end())->getNumOperands() > 0) {
            //     auto last_alter_inst = ----alterBB->end();
            //     errs() << *alterBB2->begin() << '\n';
            //     errs() << *last_alter_inst << '\n';
            //     for (auto opi = alterBB2->begin()->op_begin(), ope = alterBB2->begin()->op_end(); opi != ope; ++opi) {
            //         Instruction *def = dyn_cast<Instruction>(*opi);
                    
            //         // this checks if def is used in the last inst in alterBB
            //         if (def && &(*last_alter_inst) == &(*def)) {
            //             // create and insert garbage instruction in obfBB
            //             Instruction *bs_whatever = def->clone();
            //             obfBB->getInstList().insert(obfBB->begin(), bs_whatever);
                        
            //             // create phi node to insert at beginning of dopBB2
            //             PHINode *phi;
            //             Twine *phi_twine = new Twine("phi3");
            //             phi = PHINode::Create(def->getType(), 2, "", &(*ii));
            //             phi->addIncoming(bs_whatever, obfBB);
            //             phi->addIncoming(def, def->getParent());
            //             *opi = (Value*) phi;
            //             // assign the alterBB2 def to the phi now representing that value
            //             dop2BBPHI[def] = phi;

            //             // errs() << *obfBB2->begin() << '\n';
            //             // errs() << &*obfBB2->begin() << '\n';
            //             auto obfBB2_load_val = (Value*) &(*obfBB2->begin());
            //             // errs() << *obfBB2_load_val;
            //             // create a phi node in postBB 
            //             PHINode *phi1;
            //             Twine *phi_twine1 = new Twine("phi35");
            //             phi1 = PHINode::Create(def->getType(), 2, "", &(*postBB->begin()));
            //             phi1->addIncoming(obfBB2_load_val, obfBB2);
            //             phi1->addIncoming(*opi, alterBB2);
            
            //             // fix all uses of the above defs
            //             for (BasicBlock::iterator i = ++postBB->begin(), e = postBB->end(); i != e; ++i) {
            //                 for (User::op_iterator opie = i->op_begin(), opee = i->op_end(); opie != opee; ++opie) {
            //                     Instruction *def1 = dyn_cast<Instruction>(*opie);

            //                     // if our operand's definition was part of a previous phi node,
            //                     // then change it to be phi node
            //                     if (def1 && &(*obfBB2_load_val) == &(*def1)) {
            //                         *opie = (Value*) phi1;
            //                     } //if
            //                 } //for
            //             } //for
            //         } //if
                        
            //     } //for
            // } //if


            //errs() << *((----alterBB->end())->getOperand(1)) << "\n";

            // Make a loop that changes the operands that are part of a
            // phi node in dop2BB to be that new value. need to use a mapping
            // from the OG instruction to the Phi node instruction
            // for (BasicBlock::iterator i = alterBB2->begin(), e = alterBB2->end(); i != e; ++i) {
            //     for (User::op_iterator opi = i->op_begin(), ope = i->op_end(); opi != ope; ++opi) {
            //         Instruction *def = dyn_cast<Instruction>(*opi);

            //         // if our operand's definition was part of a previous phi node,
            //         // then change it to be phi node
            //         if (dop2BBPHI.find(def) != dop2BBPHI.end()) {
            //             *opi = (Value*) dop2BBPHI[def];
            //         } //if
            //     } //for
            // } //for
            

            // errs() << *preBB << '\n';
            // errs() << *obfBB << '\n';
            // errs() << *alterBB << '\n';
            // errs() << *dop2BB << "\n";
            // errs() << *obfBB2 << "\n";
            // errs() << *alterBB2 << "\n";
            // errs() << *postBB << '\n';
            // for (Function::iterator bb = F.begin(), e = F.end(); bb != e; ++bb)
            //     errs() << *bb << "\n";
            errs() << "we have obfuscated a BB in function ";
            errs().write_escaped(F.getName()) << "\n";
            numBB = 0;
            numEdges = 0;
            for (Function::iterator bb = F.begin(); bb != F.end(); ++bb) {
                ++numBB;
                for (BasicBlock *Pred : predecessors(&(*bb))) {
                    ++numEdges;
                }
            }
            errs() << "obfuscated function:\nnum BBs = " << numBB << "\nnum edges = " 
                   << numEdges << "\ncyclomatic number = " << numEdges - numBB + 2 
                   << '\n';
            return true;

        }

    };
}

char DopSeq::ID = 0;
static RegisterPass<DopSeq> X("DopSeq", "Dynamic opaque predicate obfuscation Pass for straight line code");

Pass *llvm::createDopSeq() {
  return new DopSeq();
}

Pass *llvm::createDopSeq(bool flag) {
  return new DopSeq(flag);
}
