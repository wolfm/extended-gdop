#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"
#include <typeinfo>
#include <set>
#include <string>

#include "TripDopSeq.h"
#include "Utils.h"

using namespace llvm;

// CURRENT ISSUE: Need to add Phi nodes after the first DOP, in case there are
// reassignments of previous variables in either BB
// However, since the GDOPs always evaluate to the same value, we should be
// able to ignore these differences - we know that those values will not be
// used in the middle block, so it shouldn't be an issue

namespace {
    struct TripDopSeq : public FunctionPass {
        static char ID;
        bool flag = true;
        TripDopSeq() : FunctionPass(ID) {}
        TripDopSeq(bool flag) : FunctionPass(ID) {this->flag = flag; TripDopSeq();}

        bool runOnFunction(Function &F) override {
            // errs() << "before to obf ";
            // errs().write_escaped(F.getName()) << '\n';
            if(toObfuscate(flag,&F,"dopseq")) {
                //StringRef *sr = new StringRef("fun");
                //if (F.getName().equals(*sr)) {

                errs() << "Function: ";
                errs().write_escaped(F.getName()) << '\n';
                
                // bool hasRun = false;
                // while (addTripDopSeq(F)) {
                //     hasRun = true;
                // }

                // return hasRun;
                return addTripDopSeq(F);

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
        bool addTripDopSeq(Function &F) {
            //errs() << "here\n";
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
                            // errs() << "The first store: " << *i << "\n";
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
                            if (lenObfBB + 1 < 7) { // TODO Adjust based on defined size of blocks
                                nonStoreNames.insert(ogName);
                                --bb;
                            } else {
                                hasSetEnd = true;
                                //i->getOperand(1)->setName(ogName);
                                // errs() << lenObfBB << "\n";
                                // errs() << "The second store: " << *i << "\n";
                            }
                            break;
                        }
                    }
                    if (i == e) nonStoreNames.clear();
                }
                if (hasSetEnd) {
                    break;
                }
            }
            
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
            Twine *dop1_twine = new Twine("dop1"); 
            Twine *dop2_twine = new Twine("dop2"); 
            Twine *dop3_twine = new Twine("dop3"); 
            
            AllocaInst* dop1 = new AllocaInst(Type::getInt32PtrTy(F.getContext()), 0, *dop1_twine, &(*ii));
            AllocaInst* dop2 = new AllocaInst(Type::getInt32PtrTy(F.getContext()), 0, *dop2_twine, &(*ii));
            AllocaInst* dop3 = new AllocaInst(Type::getInt32PtrTy(F.getContext()), 0, *dop3_twine, &(*ii));
            
            // store the variable's address to the dop pointers
            StoreInst* dop1st = new StoreInst(insertAlloca->getOperand(1), dop1, false, &(*ii));
            StoreInst* dop2st = new StoreInst(insertAlloca->getOperand(1), dop2, false, &(*ii));
            StoreInst* dop3st = new StoreInst(insertAlloca->getOperand(1), dop3, false, &(*ii));

            // errs() << *dop1st << '\n';

            // load the dop1's value
            LoadInst* dop1p = new LoadInst(Type::getInt32PtrTy(F.getContext()), dop1, "", false, &(*ii));
            LoadInst* dop1deref = new LoadInst(Type::getInt32Ty(F.getContext()), dop1p, "", false, &(*ii));

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
	            fixssa[&(*i)] = &(*j);
                fixssa_reverse[&(*j)] = &(*i);
            }
            
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

            // create the first dop at the end of preBB
            Twine * var3 = new Twine("dopbranch1");
            Value * rvalue = ConstantInt::get(Type::getInt32Ty(F.getContext()), 0);
            preBB->getTerminator()->eraseFromParent();
            
            ICmpInst * dopbranch1 = new ICmpInst(*preBB, CmpInst::ICMP_SGT, dop1deref, rvalue, *var3);
            
            BranchInst::Create(obfBB, alterBB, dopbranch1, preBB);
            
            // split the obfBB and alterBB with an offset
            // TODO Randomize this offset? need to know how big the BB chunks are
            BasicBlock::iterator splitpt1 = obfBB->begin(),
	                         splitalt1 = alterBB->begin();
            BasicBlock *obfBB2, *alterBB2, *obfBB3, *alterBB3;  
            
            int n = 2;
            for (BasicBlock::iterator e = obfBB->end(); splitpt1 != e && n > 0; ++splitpt1, --n) ;
	        n = 3;
            for (BasicBlock::iterator e = alterBB->end(); splitalt1 != e && n > 0; ++splitalt1, --n) ;
            Twine *var4 = new Twine("obfBB2");
            obfBB2 = obfBB->splitBasicBlock(splitpt1, *var4);
            Twine *var5 = new Twine("obfBBclone2");
            alterBB2 = alterBB->splitBasicBlock(splitalt1, *var5);


            // Create second set of splits
            BasicBlock::iterator splitpt2 = obfBB2->begin(),
	                         splitalt2 = alterBB2->begin();

            n = 2;
            for (BasicBlock::iterator e = obfBB2->end(); splitpt2 != e && n > 0; ++splitpt2, --n) ;
            n = 2;
            for (BasicBlock::iterator e = alterBB2->end(); splitalt2 != e && n > 0; ++splitalt2, --n) ;
            Twine *var40 = new Twine("obfBB3");

            obfBB3 = obfBB2->splitBasicBlock(splitpt2, *var40);

            Twine *var50 = new Twine("obfBBclone3");

            alterBB3 = alterBB2->splitBasicBlock(splitalt2, *var50);
            
            
            BasicBlock* dop2BB = BasicBlock::Create(F.getContext(), "dop2BB", &F, obfBB2);

            LoadInst* dop2p = new LoadInst(Type::getInt32PtrTy(F.getContext()), dop2, "", false, &(*dop2BB));
            LoadInst* dop2deref = new LoadInst(Type::getInt32Ty(F.getContext()), dop2p, "", false, &(*dop2BB));

            Twine * var6 = new Twine("dopbranch2");
            Value * rvalue2 = ConstantInt::get(Type::getInt32Ty(F.getContext()), 0);
            ICmpInst * dopbranch2 = new ICmpInst(*dop2BB, CmpInst::ICMP_SGT , dop2deref, rvalue, *var3);
            BranchInst::Create(obfBB2, alterBB2, dopbranch2, dop2BB);
            
            // connect obfBB and alterBB to the second dop
            obfBB->getTerminator()->eraseFromParent();
            BranchInst::Create(dop2BB, obfBB);
            alterBB->getTerminator()->eraseFromParent();
            BranchInst::Create(dop2BB, alterBB);

            // Connect 3rd DOP
            BasicBlock* dop3BB = BasicBlock::Create(F.getContext(), "dop3BB", &F, obfBB3);

            LoadInst* dop3p = new LoadInst(Type::getInt32PtrTy(F.getContext()), dop3, "", false, &(*dop3BB));
            LoadInst* dop3deref = new LoadInst(Type::getInt32Ty(F.getContext()), dop3p, "", false, &(*dop3BB));

            Twine * var60 = new Twine("dopbranch3");
            Value * rvalue30 = ConstantInt::get(Type::getInt32Ty(F.getContext()), 0);
            ICmpInst * dopbranch3 = new ICmpInst(*dop3BB, CmpInst::ICMP_SGT , dop3deref, rvalue, *var60);
            BranchInst::Create(obfBB3, alterBB3, dopbranch3, dop3BB);
            
            // connect obfBB and alterBB to the second dop
            obfBB2->getTerminator()->eraseFromParent();
            BranchInst::Create(dop3BB, obfBB2);
            alterBB2->getTerminator()->eraseFromParent();
            BranchInst::Create(dop3BB, alterBB2);
            
            // Iterate over obfBB2, finding uses of values defined in obfBB or obfBB2
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
                            // Insert PHI Node into dop2BB
                            // create phi node to insert at beginning of dopBB2
                            ii = dop2BB->begin();
                            PHINode *phi;
                            phi = PHINode::Create(def->getType(), 2, "", &(*ii));
                            phi->addIncoming(Constant::getNullValue(def->getType()), obfBB);
                            phi->addIncoming(fixssa[def], alterBB);

                            // Add cleanup phiNode in postBB
                            ii = dop3BB->begin();
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

            for (BasicBlock::iterator i = obfBB3->begin(), e = obfBB3->end() ; i != e; ++i) {
                for(User::op_iterator opi = i->op_begin(), ope = i->op_end(); opi != ope; ++opi) {
                    Instruction *def = dyn_cast<Instruction>(*opi);

                    // If the opi can't be cast to an Instruction*
                    if(def == nullptr) continue;

                    /*
                     *  If definition is in obfBB
                     *  Then insert PHI node in dop2BB, replace uses after it with the Phi node value,
                     *  No cleanup phinode necessary in postBB, because the value is necessarily defined there
                    */
                    if (def->getParent() == obfBB) {

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

                    else if (def->getParent() == obfBB2 && fixssa[def]->getParent() == alterBB2) {

                         // If PHINode not already created for this Value
                        if(auto i_phi = dop2BBPHI.find(def); i_phi == dop2BBPHI.end()) {
                            // Insert PhiNode into dop2BB
                            PHINode *phi;
                            ii = dop3BB->begin();
                            phi = PHINode::Create(def->getType(), 2, "", &(*ii));
                            phi->addIncoming(def, def->getParent());
                            phi->addIncoming(fixssa[def], fixssa[def]->getParent());
                            dop2BBPHI[def] = phi;
                        }

                        // Replace operand in obfBB2 and alterBB2 with the Value of the PHINode
                        *opi = (Value*) dop2BBPHI[def];
                        if(fixssa[&(*i)]->getParent() != alterBB2) {
                            fixssa[&(*i)]->setOperand(opi->getOperandNo(), (Value*) dop2BBPHI[def]);
                        }
                    }

                    else if (def->getParent() == obfBB2 && fixssa[def]->getParent() == alterBB) {

                        // If PHINode not already created for this Value
                        if(auto i_phi = dop2BBPHI.find(def); i_phi == dop2BBPHI.end()) {
                            // Insert PHI Node into dop2BB
                            // create phi node to insert at beginning of dopBB2
                            ii = dop2BB->begin();
                            PHINode *phi;
                            phi = PHINode::Create(def->getType(), 2, "", &(*ii));
                            phi->addIncoming(Constant::getNullValue(def->getType()), obfBB);
                            phi->addIncoming(fixssa[def], alterBB);

                            // Add cleanup phiNode in postBB
                            ii = dop3BB->begin();
                            PHINode *phi2;
                            phi2 = PHINode::Create(def->getType(), 2, "", &(*ii));
                            phi2->addIncoming( (Value*) def, obfBB2);
                            phi2->addIncoming( (Value*) phi, alterBB2);

                            // Add to map
                            dop2BBPHI[def] = phi;
                            dop2BBPHI[(Instruction *)phi] = phi2;
                        }
                        
                        // Replace operand in alterBB2 ONLY with the Value of the PHINode
                        *opi = (Value*) dop2BBPHI[def];
                        if(fixssa[&(*i)]->getParent() != alterBB3) {
                            fixssa[&(*i)]->setOperand(opi->getOperandNo(), (Value*) dop2BBPHI[def]);
                        }
                    }

                    else if (def->getParent() == obfBB3 && fixssa[def]->getParent() == alterBB2) {

                        // If PHINode not already created for this Value
                        if(auto i_phi = dop2BBPHI.find(def); i_phi == dop2BBPHI.end()) {
                            // Insert PHI Node into dop2BB
                            // create phi node to insert at beginning of dopBB2
                            ii = dop3BB->begin();
                            PHINode *phi;
                            phi = PHINode::Create(def->getType(), 2, "", &(*ii));
                            phi->addIncoming(Constant::getNullValue(def->getType()), obfBB2);
                            phi->addIncoming(fixssa[def], alterBB2);

                            // Add cleanup phiNode in postBB
                            ii = postBB->begin();
                            PHINode *phi2;
                            phi2 = PHINode::Create(def->getType(), 2, "", &(*ii));
                            phi2->addIncoming( (Value*) def, obfBB3);
                            phi2->addIncoming( (Value*) phi, alterBB3);

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
                    //errs() << "opname: " << i->getOpcodeName() << '\n';
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
                        // errs() << "Case 1: swapping out operand " << *opi << "\n";
                        *opi = (Value*) dop2BBPHI[def];
                        // errs() << "operand is now " << *opi << "\n";
                    }
                    // If both possible operands come from obfBB2 and alterBB2, can makea phi node in dop3BB
                    else if (def->getParent() == obfBB2 && fixssa[def]->getParent() == alterBB2) {
                        if (auto i_phi = dop2BBPHI.find(def); i_phi == dop2BBPHI.end()) {
                            PHINode *phi;
                            ii = dop3BB->begin();
                            phi = PHINode::Create(def->getType(), 2, "", &(*ii));
                            phi->addIncoming(def, def->getParent());
                            phi->addIncoming(fixssa[def], fixssa[def]->getParent());
                            dop2BBPHI[def] = phi;
                        }                        

                        //set operand for postBB use
                        // errs() << "Case 2: swapping out operand " << *opi << "\n";
                        *opi = (Value*) dop2BBPHI[def];
                        // errs() << "operand is now " << *opi << "\n";
                    }
                    // If both possible operands come from obfBB3 and alterBB3, can makea phi node in postBB
                    // If both possible operands come from obfBB2 and alterBB2, can makea phi node in dop3BB
                    else if (def->getParent() == obfBB3 && fixssa[def]->getParent() == alterBB3) {
                        if (auto i_phi = dop2BBPHI.find(def); i_phi == dop2BBPHI.end()) {
                            PHINode *phi;
                            ii = postBB->begin();
                            phi = PHINode::Create(def->getType(), 2, "", &(*ii));
                            phi->addIncoming(def, def->getParent());
                            phi->addIncoming(fixssa[def], fixssa[def]->getParent());
                            dop2BBPHI[def] = phi;
                        }                        

                        //set operand for postBB use
                        // errs() << "Case 3: swapping out operand " << *opi << "\n";
                        *opi = (Value*) dop2BBPHI[def];
                        // errs() << "operand is now " << *opi << "\n";
                    }
                    // Special case 1 - one def'n in obfBB2 and one in alterBB - Create dummy phi in dop2BB and cleanup in dop3BB
                    else if (def->getParent() == obfBB2 && fixssa[def]->getParent() == alterBB) {
                        if (auto i_phi = dop2BBPHI.find(def); i_phi == dop2BBPHI.end() && dop2BBPHI.find((Instruction *)&(*i_phi)) == dop2BBPHI.end()) {
                            ii = dop2BB->begin();
                            PHINode *phi;
                            phi = PHINode::Create(def->getType(), 2, "", &(*ii));
                            phi->addIncoming(Constant::getNullValue(def->getType()), obfBB);
                            phi->addIncoming(fixssa[def], alterBB);

                            // Add cleanup phiNode in postBB
                            ii = dop3BB->begin();
                            PHINode *phi2;
                            phi2 = PHINode::Create(def->getType(), 2, "", &(*ii));
                            phi2->addIncoming( (Value*) def, obfBB2);
                            phi2->addIncoming( (Value*) phi, alterBB2);

                            // Add to map
                            dop2BBPHI[def] = phi;
                            dop2BBPHI[(Instruction *)phi] = phi2;
                        }

                        // errs() << "Special case: swapping out operand " << *opi << "\n";
                        *opi = (Value*) dop2BBPHI[dop2BBPHI[def]];
                        // errs() << "operand is now " << *opi << "\n";
                    }
                    // Special case 2 - one def'n in obfBB3 and one in alterBB2 - Create dummy phi in dop3BB and cleanup in postBB
                    else if (def->getParent() == obfBB3 && fixssa[def]->getParent() == alterBB2) {
                        if (auto i_phi = dop2BBPHI.find(def); i_phi == dop2BBPHI.end() && dop2BBPHI.find((Instruction *)&(*i_phi)) == dop2BBPHI.end()) {
                            ii = dop3BB->begin();
                            PHINode *phi;
                            phi = PHINode::Create(def->getType(), 2, "", &(*ii));
                            phi->addIncoming(Constant::getNullValue(def->getType()), obfBB2);
                            phi->addIncoming(fixssa[def], alterBB2);

                            // Add cleanup phiNode in postBB
                            ii = postBB->begin();
                            PHINode *phi2;
                            phi2 = PHINode::Create(def->getType(), 2, "", &(*ii));
                            phi2->addIncoming( (Value*) def, obfBB3);
                            phi2->addIncoming( (Value*) phi, alterBB3);

                            // Add to map
                            dop2BBPHI[def] = phi;
                            dop2BBPHI[(Instruction *)phi] = phi2;
                        }

                        // errs() << "Special case 2: swapping out operand " << *opi << "\n";
                        *opi = (Value*) dop2BBPHI[dop2BBPHI[def]];
                        // errs() << "operand is now " << *opi << "\n";
                    }

                }

            }

            // errs() << *preBB << '\n';
            // errs() << *obfBB << '\n';
            // errs() << *alterBB << '\n';
            // errs() << *dop2BB << "\n";
            // errs() << *obfBB2 << "\n";
            // errs() << *alterBB2 << "\n";
            // errs() << *dop3BB << "\n";
            // errs() << *obfBB3 << "\n";
            // errs() << *alterBB3 << "\n";
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

char TripDopSeq::ID = 0;
static RegisterPass<TripDopSeq> X("TripDopSeq", "Dynamic opaque predicate obfuscation Pass for straight line code x3");

Pass *llvm::createTripDopSeq() {
  return new TripDopSeq();
}

Pass *llvm::createTripDopSeq(bool flag) {
  return new TripDopSeq(flag);
}
